#include <iostream>
#include <fstream>
#include <exception>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "commons.hpp"
#include "my_huffman.hpp"

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include "my_send_recv.h"
}

#define LISTEN_PORT 1732

int sockfd = 0;
int clientfd = 0;
char welcome_msg[] = "Welcome to my netprog hw2 FTP server\n";
static void sigint_safe_exit(int sig);
static const void *get_in_addr(const struct sockaddr &sa);
static uint16_t get_in_port(const struct sockaddr &sa);
static int start_server();
static int welcome(const struct sockaddr &client_addr);
static int recieve_file(std::vector<std::string> &cmd, const char *orig_cmd);
static int serve_client();

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

        serve_client();

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

static void sigint_safe_exit(int sig)
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

static const void *get_in_addr(const struct sockaddr &sa)
{
  if (sa.sa_family == AF_INET) {
    return &(reinterpret_cast<const struct sockaddr_in *>(&sa)->sin_addr);
  }
  return &(reinterpret_cast<const struct sockaddr_in6 *>(&sa)->sin6_addr);
}

static uint16_t get_in_port(const struct sockaddr &sa)
{
    if (sa.sa_family == AF_INET) {
        return ntohs(reinterpret_cast<const struct sockaddr_in *>(&sa)->sin_port);
    }
    return ntohs(reinterpret_cast<const struct sockaddr_in6 *>(&sa)->sin6_port);
}

static int start_server()
{
    int addr_family = AF_INET6;
    sockfd = socket(PF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cout << "Fail to open IPv6 socket. Fallback to IPv4 socket." << std::endl;
        addr_family = AF_INET;
        sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return sockfd;
        }
    }

    int status;

    if (addr_family == AF_INET6) {
        int no = 0;
        status = setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &no, sizeof (no));
        if (status != 0) {
            std::cout << "Fail to set dual stack socket. Fallback to IPv4 socket." << std::endl;
            close(sockfd);

            addr_family = AF_INET;
            sockfd = socket(PF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                perror("socket");
                return sockfd;
            }
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

static int welcome(const struct sockaddr &client_addr)
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

static int recieve_file(std::vector<std::string> &cmd, const char *orig_cmd)
{
    using namespace std;

    if (cmd.size() < 3) {
        cout << "Invalid command recieved. Terminating connection..." << endl;
        return -1;
    }

    int filesize;
    try {
        filesize = stoi(cmd[1]);
    }
    catch (exception &e) {
        cout << "Invalid command recieved. Terminating connection..." << endl;
        return -1;
    }

    const char *filename_c_str = strstr(orig_cmd, cmd[2].c_str());
    if (filename_c_str == NULL) {
        cout << "Invalid command recieved. Terminating connection..." << endl;
        return -1;
    }
    string filename = filename_c_str;
    string codefilename = filename + ".code";

    fstream codefile(codefilename, fstream::out | fstream::binary | fstream::trunc);
    if (!codefile.is_open()) {
        cout << "Failed to open file " << codefilename << "." << endl;
        cout << "An error has occurred. Terminating connection..." << endl;
        return -1;
    }

    cout << "Recieving " << filename << " with length " << filesize << " bytes..." << endl;

    int status = -1;
    int recieved = 0;
    while (recieved < filesize) {
        uint8_t buf[1024];
        int buflen = (filesize - recieved < 1024) ? (filesize - recieved) : 1024;
        status = my_recv_data(clientfd, buf, &buflen);
        if (status < 0) {
            perror("my_recv_data");
            break;
        }

        if (buflen == 0) {
            cout << "Connection closed by peer." << endl;
            status = -1;
            break;
        }

        recieved += buflen;
        codefile.write(reinterpret_cast<const char *>(&buf), static_cast<streamsize>(buflen));
    }

    if (status < 0) {
        cout << "An error has occurred. Terminating connection..." << endl;
        return -1;
    }

    string response = "OK " + to_string(filesize) + " bytes recieved.\n";
    int reslen = static_cast<int>(response.size());
    status = my_send(clientfd, response.c_str(), &reslen);
    if (status < 0) {
        perror("my_send");
        cout << "An error has occurred. Terminating connection..." << endl;
        return -1;
    }

    cout << response;

    fstream file(filename, fstream::out | fstream::binary | fstream::trunc);
    if (!file.is_open()) {
        cout << "Failed to open file " << filename << "." << endl;
        cout << "An error has occurred. Terminating connection..." << endl;
        return -1;
    }

    codefile.close();
    codefile.open(codefilename, fstream::in | fstream::binary);
    my_huffman::huffman_decode decode(codefile);
    decode.write(file);

    codefile.close();
    codefile.open(codefilename, fstream::out | fstream::binary | fstream::trunc);
    int char_code = 0;
    for (auto &code : decode.char_table) {
        string s(code.size(), '0');
        for (unsigned int i = 0; i < code.size(); ++i) {
            if (code[i]) {
                s[i] = '1';
            }
        }
        codefile << char_code++ << ": " << s << endl;
    }

    cout << "Huffman Code is saved in " << codefilename << " ." << endl;
    return 0;
}

static int serve_client()
{
    using namespace std;

    while (true) {
        char orig_cmd[MAX_CMD];
        int cmdlen = MAX_CMD;
        int status = my_recv_cmd(clientfd, orig_cmd, &cmdlen);
        if (status > 0) {
            if (cmdlen == 0) {
                // cmdlen == 0 means connection closed by peer.
                break;
            }
            cout << "Invalid command recieved. Terminating connection..." << endl;
            return -1;
        }
        else if (status < 0) {
            perror("my_recv_cmd");
            return -1;
        }

        orig_cmd[cmdlen - 1] = '\0';

        vector<string> cmd = parse_command(orig_cmd);
        if (cmd.size() == 0) {
            continue;
        }

        if (cmd[0] == "send") {
            if (recieve_file(cmd, orig_cmd) < 0) {
                return -1;
            }
        }
        else {
            cout << "Invalid command recieved. Terminating connection..." << endl;
            return -1;
        }
    }

    return 0;
}
