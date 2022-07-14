#include <sys/types.h>

#define LINE_MAX_LEN 2048
struct RecvBuf {
	char buf[LINE_MAX_LEN];
	char *read_ptr;
	int remain_count;
};

ssize_t recv_buf_get_line(struct RecvBuf *rb, int fd, char *line);

/// Send \a n bytes to \a fd with MSG_NOSIGNAL.
/**
 *  On error, \return -1 and sets errno.
 */
ssize_t sendn(int fd, const void *buf, size_t n);
