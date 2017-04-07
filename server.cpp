#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include "funcs.h"
}

#define LISTEN_PORT 1732
#define MAX_CMD 512

int sockfd;
int clientfd;
char welcome_msg[] = "Welcome to my netprog hw2 FTP server\n";

const void *get_in_addr(const struct sockaddr &sa)
{
  if (sa.sa_family == AF_INET) {
    return &(reinterpret_cast<const struct sockaddr_in *>(&sa)->sin_addr);
  }
  return &(reinterpret_cast<const struct sockaddr_in6 *>(&sa)->sin6_addr);
}

uint16_t get_in_port(const struct sockaddr &sa)
{
    if (sa.sa_family == AF_INET) {
        return ntohs(reinterpret_cast<const struct sockaddr_in *>(&sa)->sin_port);
    }
    return ntohs(reinterpret_cast<const struct sockaddr_in6 *>(&sa)->sin6_port);
}

int start_server()
{
    int addr_family = AF_INET6;
    sockfd = socket(PF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0) {
        addr_family = AF_INET;
        sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return sockfd;
        }
    }

    int status;
    int no = 0;
    status = setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &no, sizeof (no));
    if (status != 0) {
        std::cerr << "Fail to set dual stack socket." << std::endl;
        close(sockfd);

        addr_family = AF_INET;
        sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return sockfd;
        }
    }

    if (addr_family == AF_INET6) {
        struct sockaddr_in6 any_addr = {};
        any_addr.sin6_family = AF_INET6;
        any_addr.sin6_port = htons(LISTEN_PORT);

        status = bind(sockfd, reinterpret_cast<const struct sockaddr *>(&any_addr), sizeof (any_addr));
    }
    else {
        struct sockaddr_in any_addr = {};
        any_addr.sin_family = AF_INET;
        any_addr.sin_port = htons(LISTEN_PORT);

        status = bind(sockfd, reinterpret_cast<const struct sockaddr *>(&any_addr), sizeof (any_addr));
    }
    
    if (status < 0) {
        perror("bind");
        return status;
    }

    status = listen(sockfd, 1);
    if (status < 0) {
        perror("listen");
        return status;
    }

    std::cout << "Start listening at port " << LISTEN_PORT << "." << std::endl;

    return 0;
}

int welcome(const struct sockaddr &client_addr)
{
    char client_addr_p[INET6_ADDRSTRLEN] = {};
    if (inet_ntop(client_addr.sa_family, get_in_addr(client_addr),
    client_addr_p, sizeof (client_addr_p)) == NULL) {
        perror("inet_ntop");
        return -1;
    }

    int offset = (memcmp(client_addr_p, "::ffff:", 7) == 0) ? 7 : 0;

    std::cout << "Connection from " << (client_addr_p + offset) << " port " <<
    get_in_port(client_addr) << " protocol SOCK_STREAM(TCP) accepted." << std::endl;
    
    int msglen = (int) strlen(welcome_msg);
    return my_send(clientfd, welcome_msg, &msglen);
}

void sigint_safe_exit(int sig)
{
    if (clientfd > 2) {
        close(clientfd);
    }
    if (sockfd > 2) {
        close(sockfd);
    }
    std::cerr << "Interrupt." << std::endl;
    exit(1);
}

int main()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigint_safe_exit;
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    using namespace std;

    int status = start_server();
    if (status != 0) {
        if (sockfd > 2) {
            close(sockfd);
        }
        cerr << "Fail to start server." << endl;
        exit(1);
    }
    
    struct sockaddr_storage client_addr = {};
    socklen_t client_addr_size = sizeof (client_addr);

    clientfd = accept(sockfd, reinterpret_cast<struct sockaddr *>(&client_addr), &client_addr_size);
    while (clientfd > 0) {
        welcome(reinterpret_cast<struct sockaddr &>(client_addr));

        while (1) {
            char cmd[256];
            int cmdlen = 255;
            cmd[255] = '\0';
            int status = my_recv_cmd(clientfd, cmd, &cmdlen);
            if (status > 0) {
                if (cmdlen > 0) {
                    cout << "Invalid command recieved. Terminating connection..." << endl;
                }
                break;
            }
            else if (status < 0) {
                perror("my_recv_cmd");
                break;
            }
            cmd[cmdlen - 1] = '\0';
            cout << cmd << endl;
            cmd[cmdlen - 1] = '\n';
            my_send(clientfd, cmd, &cmdlen);
        }

        close(clientfd);
        my_clean_buf();
        cout << "Connection terminated." << endl;

        clientfd = accept(sockfd, reinterpret_cast<struct sockaddr *>(&client_addr), &client_addr_size);
    }

    if (clientfd < 0) {
        perror("accept");
    }

    close(sockfd);
    
    return 1;
}
