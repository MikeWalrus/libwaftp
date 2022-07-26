#ifndef _SOCKET_UTIL_H
#define _SOCKET_UTIL_H

#include <sys/types.h>

#define LINE_MAX_LEN 1024
struct RecvBuf {
	char buf[LINE_MAX_LEN];
	char *read_ptr;
	int remain_count;
};

void recv_buf_init(struct RecvBuf *rb);

/// Gets one line(ended by LF) from a socket \a fd using \rb.
/**
 *  The LF is copied if received.
 *  The result \a line is always '\0' terminated.
 *  \return the length of \a line (without trailing '\0') on success
 *  0 when the connection is closed, or -1 otherwise.
 */
ssize_t recv_buf_get_line(struct RecvBuf *rb, int fd, unsigned char *line);

/// Sends \a n bytes to \a fd with MSG_NOSIGNAL.
/**
 *  On error, \return -1 and sets errno.
 */
ssize_t sendn(int fd, const void *buf, size_t n);

/// Receives \a data from \a fd until the connection is closed.
/**
 *  Caller should remember to free the buffer.
 *  \return -1 if `recv` or memory allocation fails and sets `errno`.
 */
ssize_t recv_all(int fd, char **data);

#endif
