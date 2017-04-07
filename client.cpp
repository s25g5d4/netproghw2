#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>

#include "funcs.h"
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

std::vector<std::string> parse_command()
{
    using namespace std;

    cout << "> " << flush;

    string cmd_str;
    getline(cin, cmd_str);

    stringstream ss(cmd_str);
    return vector<string>(istream_iterator<string>(ss), istream_iterator<string>{});
}

int run_login(std::vector<std::string> cmd)
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

int main()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigint_safe_exit;
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    using namespace std;

    while (true) {
        vector<string> cmd = parse_command();

        if (cmd.size() == 0) {
            continue;
        }

        if (cmd[0] == "login") {
            run_login(cmd);
        }
        else if (cmd[0] == "send") {
            if (sockfd <= 2) {
                cout << "You are not logged in yet." << endl;
                continue;
            }

            string data;
            for (auto it = cmd.begin() + 1; it != cmd.end(); ++it) {
                data += *it + " ";
            }
            if (data.size() > 0) {
                data.replace(data.end() - 1, data.end(), 1, '\n');
            }
            else {
                data = "\n";
            }
            cout << data << endl;
            int datalen = (int) data.size();
            int status = my_send(sockfd, data.c_str(), &datalen);
            if (status < 0) {
                perror("my_send");
                break;
            }

            char msg[256];
            int msglen = 255;
            status = my_recv_cmd(sockfd, msg, &msglen);
            if (status > 0) {
                cout << "Invalid response. Terminate conneciton." << endl;
                break;
            }
            else if (status < 0) {
                perror("my_recv_cmd");
                break;
            }

            msg[msglen > 0 ? msglen - 1 : 0] = '\0';
            cout << msg << endl;
        }
        else if (cmd[0] == "logout" || cmd[0] == "exit") {
            break;
        }
        else {
            cout << "Invalid command:";
            for (auto &str : cmd) cout << " " << str;
            cout << endl;
            continue;
        }

    }

    if (sockfd > 2) {
        close(sockfd);
    }

    cout << "Goodbye." << endl;
    return 0;
}
