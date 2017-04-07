#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "funcs.h"

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main()
{
    struct addrinfo hints = {};
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res;
    if (getaddrinfo(NULL, "1732", &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }
    
    struct addrinfo *p;
    int sockfd = -1;
    for (p = res; p != NULL; p = p->ai_next) {
        if (p->ai_family != AF_INET6) continue;
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            perror("socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            perror("bind");
            close(sockfd);
            sockfd = -1;
            continue;
        }

        break;
    }

    if (p == NULL) {
        exit(1);
    }

    freeaddrinfo(res);
    
    if (listen(sockfd, 1) < 0) {
        perror("listen");
        exit(1);
    }

    char welcome_msg[] = "Welcome to my netprog hw2 file server\n";

    int clientfd;
    struct sockaddr_storage client_addr = {};
    socklen_t client_addr_size = sizeof (client_addr);

    clientfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_size);
    while (clientfd > 0) {
        char client_addr_p[INET6_ADDRSTRLEN] = {};
        if (inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *) &client_addr),
        client_addr_p, sizeof (client_addr_p)) == NULL) {
            perror("inet_ntop");
            exit(1);
        }
        printf("Connection from %s port %hu protocol SOCK_STREAM(TCP) accepted.\n",
               client_addr_p, ((struct sockaddr_in *) &client_addr)->sin_port);
        
        int msglen = (int) strlen(welcome_msg);
        my_send(clientfd, welcome_msg, &msglen);

        char * large_data = (char *) malloc(1024);
        int datalen = 1024;
        memset(large_data, '0', 1024);
        large_data[1024 - 1] = '\n';

        my_send(clientfd, large_data, &datalen);

        close(clientfd);
        printf("Connection terminated.\n");

        clientfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_size);
    }

    if (clientfd < 0) {
        perror("accept");
    }
    close(sockfd);
    
    return 1;
}
