#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>

extern "C" {
#include "funcs.h"
}

int sockfd = 0;

int login(const char * addr, const char * port)
{

    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res;
    if (getaddrinfo(addr, port, &hints, &res) != 0) {
        return -1;
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

    freeaddrinfo(res);

    if (p == NULL) {
        return -1;
    }
    

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

int main()
{
    using namespace std;

    while (true) {
        cout << "> " << std::flush;

        string cmd_str;
        getline(cin, cmd_str);

        stringstream ss(cmd_str);
        std::vector<std::string> cmd(istream_iterator<string>(ss), istream_iterator<string>{});

        if (cmd.size() == 0) {
            continue;
        }

        if (cmd[0] == "login") {
            if (cmd.size() < 3) {
                cout << "Invalid command:";
                for (auto &str : cmd) cout << " " << str;
                cout << endl;
                continue;
            }

            if (sockfd > 2) {
                cout << "The connection has been established." << endl;
                continue;
            }

            if (login(cmd[1].c_str(), cmd[2].c_str()) < 0) {
                cout << "Fail to run command." << endl;
                continue;
            }
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
