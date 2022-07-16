#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"

#include "cmd.h"
#include "ftp.h"
#include "telnet.h"

#define CMD_BUF_LEN 2

enum GetReplyResult {
	GET_REPLY_NETWORK_ERROR, // errno
	GET_REPLY_TELNET_ERROR, // errno
	GET_REPLY_CLOSED,
	GET_REPLY_SYNTAX_ERROR,
	GET_REPLY_OK
};

const char *get_reply_err_msg[] = {
	[GET_REPLY_SYNTAX_ERROR] = "get_reply: Syntax error.",
	[GET_REPLY_CLOSED] = "get_reply: Connection is closed.",
	[GET_REPLY_TELNET_ERROR] = "get_reply: Cannot send a telnet command: ",
	[GET_REPLY_NETWORK_ERROR] = "get_reply: Error while receiving reply: "
};

static enum GetReplyResult get_reply(int fd, struct RecvBuf *rb,
                                     struct Reply *reply);

void get_reply_result_to_err_msg(enum GetReplyResult result, char *err_msg,
                                 size_t len);

/// Send a command and get its primary reply.
/**
 *  /return -1 on error.
 */
static int send_command(int fd, struct Reply *reply, char *err_msg,
                        const char *fmt, ...)
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
			strncpy(err_msg, __func__, ERR_MSG_MAX_LEN);
			size_t err_len = sizeof(__func__);
			strerror_r(errno, err_msg + err_len,
			           ERR_MSG_MAX_LEN - err_len);
		}
		ret = -1;
		goto clean_up;
	}
	struct RecvBuf rb;
	recv_buf_init(&rb);
	enum GetReplyResult result = get_reply(fd, &rb, reply);
	if (result != GET_REPLY_OK) {
		if (err_msg) {
			strncpy(err_msg, __func__, ERR_MSG_MAX_LEN);
			size_t err_len = sizeof(__func__);
			get_reply_result_to_err_msg(result, err_msg + err_len,
			                            LINE_MAX_LEN - err_len);
		}
		ret = -1;
		goto clean_up;
	}
clean_up:
	free(cmd_buf_bigger);
	return ret;
}

static enum GetReplyResult get_reply(int fd, struct RecvBuf *rb,
                                     struct Reply *reply)
{
	unsigned char line[LINE_MAX_LEN];
	ssize_t len;
	unsigned char *ptr;
	unsigned char *end;

#define GET_LINE()                                                             \
	len = recv_buf_get_line(rb, fd, line);                                 \
	if (len < 0)                                                           \
		return GET_REPLY_NETWORK_ERROR;                                \
	if (len == 0)                                                          \
		return GET_REPLY_CLOSED;                                       \
	assert(line[len - 1] ==                                                \
	       '\n'); /* TODO: Not sure about this. Let's just crash first.*/  \
	ptr = line;                                                            \
	end = ptr + len;

#define DEAL_WITH_TELNET_CMD()                                                 \
	if (try_read_telnet_cmd_and_reply(&ptr, end, fd) < 0)                  \
		return GET_REPLY_TELNET_ERROR;

#define DEAL_WITH_TELNET_CMD_TILL_END()                                        \
	ptr++;                                                                 \
	while (ptr < end) {                                                    \
		DEAL_WITH_TELNET_CMD();                                        \
		ptr++;                                                         \
	}

#define DEAL_WITH_TELNET_CMD_WITH_CHECKS(action)                               \
	if (ptr >= end)                                                        \
		action;                                                        \
	DEAL_WITH_TELNET_CMD();                                                \
	if (ptr >= end)                                                        \
		action;

	GET_LINE();
	for (size_t i = 0; i < 3; i++, ptr++) {
		DEAL_WITH_TELNET_CMD_WITH_CHECKS(
			return GET_REPLY_SYNTAX_ERROR;);
		reply->reply_codes[i] = *ptr - '0';
	}

	DEAL_WITH_TELNET_CMD_WITH_CHECKS(return GET_REPLY_OK);
	bool is_multi_line = *ptr == '-';
	DEAL_WITH_TELNET_CMD_TILL_END();

	if (!is_multi_line)
		return GET_REPLY_OK;

	// Oh, we have a multi-line reply!
	for (;;) {
		GET_LINE();
		// Look for xyz<SP>, where x, y, z are digits.
		for (size_t i = 0; i < 3; i++, ptr++) {
			DEAL_WITH_TELNET_CMD_WITH_CHECKS(goto not_last_line);
			if (!isdigit(*ptr)) {
				DEAL_WITH_TELNET_CMD_TILL_END();
				goto not_last_line;
			}
		}
		DEAL_WITH_TELNET_CMD_WITH_CHECKS(goto not_last_line);
		bool is_space = *ptr == ' ';
		DEAL_WITH_TELNET_CMD_TILL_END();
		if (!is_space)
			goto not_last_line;
		break;
	not_last_line:
		continue;
	}

	return GET_REPLY_OK;

#undef DEAL_WITH_TELNET_CMD_TILL_END
#undef DEAL_WITH_TELNET_CMD
#undef DEAL_WITH_TELNET_CMD_WITH_CHECKS
#undef GET_LINE
}

void get_reply_result_to_err_msg(enum GetReplyResult result, char *err_msg,
                                 size_t len)
{
	const char *first_part = get_reply_err_msg[result];
	ssize_t first_part_len = strlen(first_part);
	strncpy(err_msg, get_reply_err_msg[result], len);

	bool errno_involved = result == GET_REPLY_TELNET_ERROR ||
	                      result == GET_REPLY_NETWORK_ERROR;
	if (errno_involved)
		strerror_r(errno, err_msg + len, len - first_part_len);
	err_msg[len - 1] = 0;
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
