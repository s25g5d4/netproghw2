#ifndef __FUNCS_H__
#define __FUNCS_H__

/**
 * Description: Try to write `buflen` bytes of message of `buf` to `fd`.
 *              Actual bytes written will be stored in `buflen`.
 * Return: -1 if fail, and errno set to appropriate value.
 *         0 if succeed.
 */
int my_send(int fd, const void *buf, int *buflen);

/**
 * Description: Clean internal buffer. Must be called if a client exited and
 *              another client accepted.
 */
void my_clean_buf();

/**
 * Description: Try to read command from `fd` with a newline character ('\n').
 *              Read up to `buflen` bytes and write to `buf`.
 *              Actual bytes written will be stored in `buflen`.
 * Return: -1 if fail, and errno set to appropriate value.
 *         0 if read succeed and command is valid (end with '\n').
 *         1 if read succeed but command is not valid (not end with '\n') or
*          read exact `buflen` bytes but no '\n' received.
 */
int my_recv_cmd(int fd, char *buf, int *buflen);

/**
 * Description: Read binary data.
 *              Read up to `buflen` bytes and write to `buf`.
 *              Actual bytes written will be stored in `buflen`.
 * Return: -1 if fail, and errno set to appropriate value.
 *         0 if read succeed.
 */
int my_recv_data(int fd, void *buf, int *buflen);

#endif