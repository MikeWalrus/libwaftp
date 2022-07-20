#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "debug.h"
#include "error.h"
#include "ftp.h"
#include "telnet.h"

#define ERR_PRINTF_REPLY(reply, fmt, ...)                                      \
	ERR_PRINTF(fmt " (%d%d%d)", ##__VA_ARGS__, reply.first, reply.second,  \
	           reply.third)

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

int send_command(int fd, struct Reply *reply, struct ErrMsg *err,
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

	if (sendn(fd, cmd_buf, len + 2) != len + 2) {
		strerror_r(errno, err->msg, ERR_MSG_MAX_LEN);
		goto fail;
	}
	debug("[O] %s", cmd_buf);
	struct RecvBuf rb;
	recv_buf_init(&rb);
	enum GetReplyResult result = get_reply(fd, &rb, reply);
	if (result != GET_REPLY_OK) {
		get_reply_result_to_err_msg(result, err->msg, LINE_MAX_LEN);
		goto fail;
	}
clean_up:
	free(cmd_buf_bigger);
	return ret;
fail:
	ERR_WHERE_PRINTF("%s(%s)", __func__, cmd_buf);
	ret = -1;
	goto clean_up;
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
	debug("[I] %s", line);                                                 \
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

int get_connection_greetings(int fd, struct RecvBuf *rb, struct ErrMsg *err)
{
	for (;;) {
		struct Reply reply;
		enum GetReplyResult result = get_reply(fd, rb, &reply);
		if (result != GET_REPLY_OK) {
			get_reply_result_to_err_msg(result, err->msg,
			                            ERR_MSG_MAX_LEN);
			goto fail;
		}
		enum ReplyCode1 first = reply.first;
		if (first == POS_PRE)
			continue;
		if (first == POS_COM)
			return 0;
		if (first == NEG_TRAN_COM) {
			ERR_PRINTF_REPLY(reply,
			                 "The server says it's unavailable.")
			goto fail;
		}
		ERR_PRINTF_REPLY(reply, "Unexpected reply.");
		goto fail;
	}
fail:
	ERR_WHERE();
	return -1;
}

int perform_login_sequence(const struct LoginInfo *l, int fd,
                           struct RecvBuf *rb, struct ErrMsg *err)
{
	/*
	RFC 959 Page 57:
                              1
        +---+   USER    +---+------------->+---+
        | B |---------->| W | 2       ---->| E |
        +---+           +---+------  |  -->+---+
                         | |       | | |
                       3 | | 4,5   | | |
           --------------   -----  | | |
          |                      | | | |
          |                      | | | |
          |                 ---------  |
          |               1|     | |   |
          V                |     | |   |
        +---+   PASS    +---+ 2  |  ------>+---+
        |   |---------->| W |------------->| S |
        +---+           +---+   ---------->+---+
                         | |   | |     |
                       3 | |4,5| |     |
           --------------   --------   |
          |                    | |  |  |
          |                    | |  |  |
          |                 -----------
          |             1,3|   | |  |
          V                |  2| |  |
        +---+   ACCT    +---+--  |   ----->+---+
        |   |---------->| W | 4,5 -------->| F |
        +---+           +---+------------->+---+
	*/

	struct Reply reply;
	const char *cmd;
	const char *info;
	enum ReplyCode1 first;
	// USER cmd
	if (!l->username) {
		info = "A username";
		goto info_needed;
	}
	if (send_command(fd, &reply, err, "USER %s", l->username) < 0)
		return -1;
	cmd = "USER";
	first = reply.first;
	if (first == POS_COM)
		goto succeed;
	if (first == POS_PRE)
		goto error;
	if (first == NEG_TRAN_COM || first == NEG_PERM_COM) {
		ERR_PRINTF_REPLY(
			reply,
			"Failure: Login failed after sending the username \"%s\".",
			l->username);
		// TODO: Extract more information from the reply.
		goto fail;
	}
	if (first != POS_INT) {
		ERR_PRINTF_REPLY(
			reply,
			"Unexpected reply after sending the username \"%s\".",
			l->username);
		goto fail;
	}

	// PASS cmd
	debug("[INFO] Password needed to login.\n");
	if (!l->password) {
		info = "Your password";
		goto info_needed;
	}
	if (send_command(fd, &reply, err, "PASS %s", l->password) < 0)
		return -1;
	cmd = "PASS";
	first = reply.first;
	if (first == POS_COM)
		goto succeed;
	if (first == POS_PRE)
		goto error;
	if (first == NEG_TRAN_COM || first == NEG_PERM_COM) {
		ERR_PRINTF_REPLY(
			reply,
			"Failure: Login failed after sending the password.");
		// TODO: Extract more information from the reply.
		goto fail;
	}
	if (first != POS_INT) {
		ERR_PRINTF_REPLY(
			reply, "Unexpected reply after sending the password.");
		goto fail;
	}

	// ACCT cmd
	debug("[INFO] Account information needed to login.\n");
	if (!l->account_info) {
		info = "Your account information";
		goto info_needed;
	}
	if (send_command(fd, &reply, err, "ACCT %s", l->account_info) < 0)
		return -1;
	cmd = "ACCT";
	first = reply.first;
	if (first == POS_COM)
		goto succeed;
	if (first == POS_PRE || first == POS_INT)
		goto error;
	if (first == NEG_TRAN_COM || first == NEG_PERM_COM) {
		ERR_PRINTF_REPLY(
			reply,
			"Failure: Login failed after sending the account information.");
		// TODO: Extract more information from the reply.
		goto fail;
	}

	return 0;
info_needed:
	ERR_PRINTF_REPLY(reply, "%s is needed to log into this server.", info);
	goto fail;
error:
	ERR_PRINTF_REPLY(
		reply,
		"Error: The server shouldn't send this reply to my %s command.",
		cmd);
fail:
	debug("[WARNING] Login failed.\n");
	ERR_WHERE();
	return -1;
succeed:
	debug("[INFO] Login succeeded.\n");
	return 0;
}
