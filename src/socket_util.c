#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debug.h"
#include "socket_util.h"

ssize_t sendn(int fd, const void *buf, size_t n)
{
	size_t n_remain = n;
	const char *p = buf;
	ssize_t n_written;

	while (n_remain) {
		if ((n_written = send(fd, p, n_remain, MSG_NOSIGNAL)) <= 0) {
			if (n_written < 0 && errno == EINTR)
				n_written = 0; /* we need to send() again */
			else
				return -1;
		}
		n_remain -= n_written;
		p += n_written;
	}
	return n;
}

ssize_t try_recv(int fd, char *buf, size_t size)
{
	for (;;) {
		ssize_t n = recv(fd, buf, size, MSG_NOSIGNAL);
		if (n < 0) {
			if (errno != EINTR)
				return -1;
			continue;
		}
		return n;
	}
}

void recv_buf_init(struct RecvBuf *rb)
{
	rb->remain_count = 0;
}

/// Fills the receive buffer \a rb by calling recv().
/**
 *  Only call this if everything previously stored in the buffer
 *  has been consumed.
 *  \return 1 on success, 0 when the connection is closed, or -1 otherwise.
 */
static int recv_buf_fill(struct RecvBuf *rb, int fd)
{
	for (;;) {
		if ((rb->remain_count = recv(fd, rb->buf, sizeof(rb->buf),
		                             MSG_NOSIGNAL)) < 0) {
			if (errno != EINTR)
				return -1;
			continue;
		} else if (rb->remain_count == 0) {
			return 0; // The connection has been properly closed.
		}
		rb->read_ptr = rb->buf;
		return 1;
	}
}

/// Gets one character from \a rb and put it in \a c.
/**
 *  Potentially refills the buffer.
 *  \return 1 on success, 0 when the connection is closed, or -1 otherwise.
 */
static inline int recv_buf_get_char(struct RecvBuf *rb, int fd, char *c)
{
	if (rb->remain_count <= 0) {
		int result = recv_buf_fill(rb, fd);
		if (result <= 0)
			return result;
	}
	rb->remain_count--;
	*c = *(rb->read_ptr++);
	return 1;
}

ssize_t recv_buf_get_line(struct RecvBuf *rb, int fd, unsigned char *line)
{
	int result;
	ssize_t n;
	for (n = 0; n < LINE_MAX_LEN; n++) {
		char c;
		if ((result = recv_buf_get_char(rb, fd, &c)) == 1) {
			*line++ = c;
			if (c == '\n')
				break;
			continue;
		}
		if (result == 0) {
			*line = 0;
			return n - 1;
		}
		return -1;
	}
	*line = 0;
	n++;
	return n;
}

ssize_t recv_all(int fd, char **data)
{
#define CHUNK_SIZE 1024
	size_t received = 0;
	char *buf = NULL;
	size_t capacity = CHUNK_SIZE;

	ssize_t n;

	for (;;) {
		if (received + CHUNK_SIZE + 1 >= capacity) {
			capacity *= 2;
			char *new_buf = realloc(buf, capacity);
			if (!new_buf) {
				free(buf);
				return -1;
			}
			buf = new_buf;
		}
		if ((n = recv(fd, &buf[received], CHUNK_SIZE, MSG_NOSIGNAL)) <
		    0) {
			if (errno == EINTR) {
				continue;
			}
			free(buf);
			return -1;
		}
		if (n == 0) {
			*data = buf;
			close(fd);
			buf[received] = '\0';
			return received;
		}
		// n > 0
		received += n;
	}
}
