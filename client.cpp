#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "commons.hpp"

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <libgen.h>

#include "my_send_recv.h"
}

int sockfd = 0;

int login(const char *addr, const char *port)
{
    struct addrinfo hints = {};
    struct addrinfo *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int status = getaddrinfo(addr, port, &hints, &res);
    if (status != 0) {
        return status;
    }

    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            perror("socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            perror("connect");
            close(sockfd);
            sockfd = 0;
            continue;
        }

        break;
    }

    if (p == NULL) {
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    int recv_status;

    char welcome_msg[64] = {};
    int msglen = (int) sizeof (welcome_msg);
    recv_status = my_recv_cmd(sockfd, welcome_msg, &msglen);
    if (recv_status < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            welcome_msg[msglen - 1] = '\0';
            fprintf(stderr, "Invalid command recieved: %s\n", welcome_msg);
        }
        else {
            perror("my_recv_cmd");
            return -1;
        }
    }
    else if (recv_status > 0) {
        welcome_msg[msglen - 1] = '\0';
        fprintf(stderr, "Invalid command recieved: %s\n", welcome_msg);
    }
    else {
        welcome_msg[msglen - 1] = '\0';
        printf("%s\n", welcome_msg);
    }

    return 0;
}

void sigint_safe_exit(int sig)
{
    if (sockfd > 2) {
        close(sockfd);
    }
    fprintf(stderr, "Interrupt.\n");
    exit(1);
}

int run_login(std::vector<std::string> &cmd)
{
    if (cmd.size() < 3) {
        std::cout << "Invalid command.";
        return -1;
    }

    if (sockfd > 2) {
        std::cout << "The connection has been established." << std::endl;
        return -1;
    }

    std::cout << "Connecting to " << cmd[1] << ":" << cmd[2] << std::endl;
    if (login(cmd[1].c_str(), cmd[2].c_str()) < 0) {
        std::cout << "Fail to login." << std::endl;
        return -1;
    }

    return 0;
}

int run_send(std::vector<std::string> &cmd, std::string orig_cmd)
{
    using namespace std;

    if (sockfd <= 2) {
        cout << "You are not logged in yet." << endl;
        return -1;
    }

    if (cmd.size() < 2) {
        cout << "Please provide file name." << endl;
        return -1;
    }

    string::size_type n = orig_cmd.find(cmd[1]);
    if (n == string::npos) {
        cout << "Command error." << endl;
        return -1;
    }

    string pathname(orig_cmd.begin() + n, orig_cmd.end());

    struct stat filestat;
    int status = stat(pathname.c_str(), &filestat);
    if (status < 0) {
        perror("stat");
        return -1;
    }
    if (!S_ISREG(filestat.st_mode)) {
        cout << "Not a regular file." << endl;
        return -1;
    }

    int filesize = static_cast<int>(filestat.st_size);
    ifstream file(pathname, fstream::in | fstream::binary);
    if (!file.is_open()) {
        cout << "Failed to open file." << endl;
        return -1;
    }

    char *pathname_c_str = new char[pathname.size() + 1];
    memcpy(pathname_c_str, pathname.c_str(), pathname.size() + 1);

    string filename = basename(pathname_c_str);

    delete pathname_c_str;

    string send_cmd = "send " + to_string(filesize) + " " + filename + "\n";
    int sendlen = static_cast<int>(send_cmd.size());
    status = my_send(sockfd, send_cmd.c_str(), &sendlen);
    if (status < 0) {
        perror("my_send");
        cout << "Send failed." << endl;
        return -1;
    }

    int sent = 0;
    while (sent < filesize) {
        uint8_t buf[1024];
        file.read(reinterpret_cast<char *>(&buf), sizeof (buf));
        int buflen = static_cast<int>(file.gcount());
        status = my_send(sockfd, buf, &buflen);
        if (status < 0) {
            perror("my_send");
            cout << "Send failed." << endl;
            return -1;
        }

        sent += buflen;
    }

    char msg[256];
    int msglen = 255;
    status = my_recv_cmd(sockfd, msg, &msglen);
    if (status > 0) {
        cout << "Invalid response. Terminate conneciton." << endl;
        return -1;
    }
    else if (status < 0) {
        perror("my_recv_cmd");
        return -1;
    }

    vector<string> res = parse_command(msg);
    if (res.size() < 2 || res[0] != "OK") {
        return -1;
    }
    
    try {
        sent = stoi(res[1]);
    }
    catch (exception &e) {
        cerr << e.what() << endl;
        cout << "Invalid response. Terminate conneciton." << endl;
        return -1;
    }

    cout << "OK " << sent << " bytes sent." << endl;
    return 0;
}

int main()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigint_safe_exit;
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    using namespace std;

    while (true) {
        cout << "> " << flush;
        string orig_cmd;
        getline(cin, orig_cmd);
        vector<string> cmd = parse_command(orig_cmd);
        if (cmd.size() == 0) {
            continue;
        }

        if (cmd[0] == "login") {
            run_login(cmd);
        }
        else if (cmd[0] == "send") {
            run_send(cmd, orig_cmd);
        }
        else if (cmd[0] == "logout" || cmd[0] == "exit") {
            break;
        }
        else {
            cout << "Invalid command." << endl;
            continue;
        }

    }

    if (sockfd > 2) {
        close(sockfd);
    }

    cout << "Goodbye." << endl;
    return 0;
}
