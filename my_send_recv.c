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
#include <inttypes.h>

#include "my_send_recv.h"

static uint8_t in_buf[1048576];
static int in_buflen = 0;

static int my_recv(int fd, int flags)
{
    int recieved_val = recv(fd, in_buf, sizeof (in_buf), flags);
    if (recieved_val < 0) {
        in_buflen = 0;
        return recieved_val;
    }
    in_buflen = recieved_val;

    return in_buflen;
}

void my_clean_buf()
{
    in_buflen = 0;
}

int my_send(int fd, const void *buf, int *buflen)
{
    int sent = 0;
    int send_val = 0;

    while (*buflen > sent) {
        send_val = send(fd, buf + sent, *buflen - sent, 0);
        if (send_val <= 0) {
            *buflen = sent;
            return -1;
        }

        sent += send_val;
    }

    *buflen = sent;
    return 0;
}

int my_recv_cmd(int fd, char *buf, int *buflen)
{
    int recieved_val;
    if (in_buflen == 0) {
        recieved_val = my_recv(fd, 0);
        if (recieved_val <= 0) {
            *buflen = 0;
            return (recieved_val == 0 ? 1 : -1);
        }
    }

    int recieved = 0;

    uint8_t *cmd_end = (uint8_t *) memchr(in_buf, '\n', (size_t) in_buflen);
    while (cmd_end == NULL) {
        size_t cpy_size = (size_t) (*buflen - recieved >= in_buflen) ? in_buflen : (*buflen - recieved);
        memcpy(buf + recieved, in_buf, cpy_size);
        recieved += cpy_size;
        if (*buflen <= recieved) {
            memmove(in_buf, in_buf + cpy_size, in_buflen - cpy_size);
            in_buflen -= cpy_size;
            *buflen = recieved;
            return 1;
        }

        recieved_val = my_recv(fd, 0);
        if (recieved_val <= 0) {
            *buflen = recieved;
            return (recieved_val == 0 ? 1 : -1);
        }
        cmd_end = memchr(in_buf, '\n', (size_t) in_buflen);
    }

    int end_index = (int)(cmd_end - in_buf) + 1;

    size_t cpy_size = (size_t) (*buflen - recieved >= end_index) ? end_index : (*buflen - recieved);
    memcpy(buf + recieved, in_buf, cpy_size);
    recieved += cpy_size;
    if (in_buflen > cpy_size) {
        memmove(in_buf, in_buf + cpy_size, in_buflen - cpy_size);
    }
    in_buflen -= cpy_size;
    *buflen = recieved;
    if (cpy_size < end_index) {
        return 1;
    }
    
    return 0;
}

int my_recv_data(int fd, void *buf, int *buflen)
{
    int recieved_val;
    if (in_buflen == 0) {
        recieved_val = my_recv(fd, 0);
        if (recieved_val <= 0) {
            *buflen = 0;
            return recieved_val;
        }
    }

    int recieved = 0;
    while (recieved < *buflen) {
        size_t cpy_size = (size_t) (*buflen - recieved >= in_buflen) ? in_buflen : (*buflen - recieved);
        memcpy(buf + recieved, in_buf, cpy_size);
        recieved += cpy_size;
        if (*buflen <= recieved) {
            memmove(in_buf, in_buf + cpy_size, in_buflen - cpy_size);
            in_buflen -= cpy_size;
            *buflen = recieved;
            return 0;
        }

        recieved_val = my_recv(fd, 0);
        if (recieved_val < 0) {
            *buflen = recieved;
            return recieved_val;
        }
        if (recieved_val == 0) {
            break;
        }
    }

    *buflen = recieved;
    return 0;
}
