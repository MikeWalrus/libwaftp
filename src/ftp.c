#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"

#include "ftp.h"
#include "socket_util.h"

#define CMD_BUF_LEN 2

static int get_reply(int fd);

/// Send a command and get its primary reply.
/**
 *  /return -1 on error.
 */
static int send_command(int fd, char *err_msg, const char *fmt, ...)
{
	int ret = 0;
	va_list args;
	va_start(args, fmt);
	// First try a fixed-size array to avoid malloc.
	char cmd_buf_small[CMD_BUF_LEN];
	const size_t _CMD_BUF_LEN = CMD_BUF_LEN - 2;
	char *cmd_buf_bigger = NULL;
	char *cmd_buf = cmd_buf_small;
	size_t len = vsnprintf(cmd_buf, _CMD_BUF_LEN, fmt, args);
	if (len >= _CMD_BUF_LEN) {
		cmd_buf_bigger = malloc(len + 3); // CR LF \0
		len = vsnprintf(cmd_buf_bigger, len + 1, fmt, args);
		cmd_buf = cmd_buf_bigger;
	}
	strcpy(&cmd_buf[len], "\r\n");
	va_end(args);

	if (sendn(fd, cmd_buf, len) != 0) {
		if (err_msg) {
			ssize_t err_len = snprintf(err_msg, ERR_MSG_MAX_LEN,
			                           "send_command: ");
			strerror_r(errno, err_msg + err_len,
			           ERR_MSG_MAX_LEN - err_len);
		}
		ret = -1;
		goto clean_up;
	}
	// TODO: get_reply
clean_up:
	free(cmd_buf_bigger);
	return ret;
}

static int get_reply(int fd)
{
	// TODO
}

static int getaddrinfo_ftp(const char *name, const char *service,
                           struct addrinfo **ret)
{
	const struct addrinfo hint =
		(struct addrinfo){ .ai_family = AF_UNSPEC,
		                   .ai_socktype = SOCK_STREAM };
	return getaddrinfo(name, service, &hint, ret);
}

/// Iterate \a addr_info and try to connect.
/**
 *  \return a socket descriptor on success, 0 on faliure.
 */
static int addrinfo_connect(struct addrinfo *addr_info)
{
	for (; addr_info; addr_info = addr_info->ai_next) {
		int s = socket(addr_info->ai_family, addr_info->ai_socktype,
		               addr_info->ai_protocol);
		if (s < 0)
			continue;
		if (connect(s, addr_info->ai_addr, addr_info->ai_addrlen) == 0)
			return s;
		close(s);
	}
	return 0;
}

struct UserPI *user_pi_init(const char *name, const char *service,
                            struct UserPI *user_pi, char *err_msg)
{
	int n;
	struct addrinfo *addr_info;
	if ((n = getaddrinfo_ftp(name, service, &addr_info)) != 0) {
		snprintf(err_msg, ERR_MSG_MAX_LEN, "getaddrinfo: %s",
		         gai_strerror(n));
		return NULL;
	}
	int fd = addrinfo_connect(addr_info);
	if (fd <= 0) {
		snprintf(err_msg, ERR_MSG_MAX_LEN,
		         "user_pi_init: Cannot connect to %s, %s", name,
		         service);
		freeaddrinfo(addr_info);
		return NULL;
	}
	*user_pi = (struct UserPI){ .addr_info = addr_info,
		                    .name = name,
		                    .service = service,
		                    .ctrl_fd = fd };
	return user_pi;
}
