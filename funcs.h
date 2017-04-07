#ifndef __FUNCS_H__
#define __FUNCS_H__

int my_send(int fd, const void *buf, int *buflen);

int my_recv_cmd(int fd, char *buf, int *buflen);

#endif