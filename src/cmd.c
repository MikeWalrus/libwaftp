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
#include "parse.h"
#include "telnet.h"

#define ERR_PRINTF_REPLY(reply, fmt, ...)                                      \
	ERR_PRINTF(fmt " (%s)", ##__VA_ARGS__, reply)

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

static bool is_reply_eq(struct Reply *reply, unsigned int reply_code[3])
{
	for (size_t i = 0; i < 3; i++) {
		if (reply->reply_codes[i] != reply_code[i])
			return false;
	}
	return true;
}

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

/// Look for xyz-, where x, y, z are digits.
static bool is_reply_multi_line(char short_reply[4], size_t len)
{
	if (4 > len)
		return false;
	return short_reply[3] == '-';
}

/// Look for xyz<SP>, where x, y, z are digits.
static bool is_reply_multi_line_last(char short_reply[4], size_t len)
{
	if (4 > len)
		return false;
	return short_reply[3] == ' ';
}

static enum GetReplyResult get_reply(int fd, struct RecvBuf *rb,
                                     struct Reply *reply)
{
	reply->len = 0;
	unsigned char line[LINE_MAX_LEN];
	ssize_t len;

#define GET_LINE()                                                             \
	len = recv_buf_get_line(rb, fd, line);                                 \
	if (len < 0)                                                           \
		return GET_REPLY_NETWORK_ERROR;                                \
	if (len == 0)                                                          \
		return GET_REPLY_CLOSED;                                       \
	debug("[I] %s", line);                                                 \
	assert(line[len - 1] ==                                                \
	       '\n'); /* TODO: Not sure about this. Let's just crash first.*/  \
	if (copy_from_telnet_line(fd, line, len, reply) < 0)                   \
		return GET_REPLY_TELNET_ERROR;

	GET_LINE();
	for (size_t i = 0; i < 3; i++) {
		if (i + 1 > reply->len)
			return GET_REPLY_SYNTAX_ERROR;
		reply->reply_codes[i] = reply->reply[i] - '0';
	}
	if (!is_reply_multi_line(reply->short_reply, reply->short_reply_len))
		return GET_REPLY_OK;

	// Oh, we have a multi-line reply!
	for (;;) {
		GET_LINE();
		if (is_reply_multi_line_last(reply->short_reply,
		                             reply->short_reply_len))
			break;
	}

	return GET_REPLY_OK;

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
			ERR_PRINTF_REPLY(reply.short_reply,
			                 "The server says it's unavailable.")
			goto fail;
		}
		ERR_PRINTF_REPLY(reply.short_reply, "Unexpected reply.");
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
	enum ReplyCode1 *first = &reply.first;
	// USER cmd
	if (!l->username) {
		info = "A username";
		goto info_needed;
	}
	if (send_command(fd, &reply, err, "USER %s", l->username) < 0)
		return -1;
	cmd = "USER";
	if (*first == POS_COM)
		goto succeed;
	if (*first == POS_PRE)
		goto error;
	if (*first == NEG_TRAN_COM || *first == NEG_PERM_COM) {
		ERR_PRINTF_REPLY(
			reply,
			"Failure: Login failed after sending the username \"%s\".",
			l->username);
		// TODO: Extract more information from the reply.
		goto fail;
	}
	if (*first != POS_INT) {
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
	if (*first == POS_COM)
		goto succeed;
	if (*first == POS_PRE)
		goto error;
	if (*first == NEG_TRAN_COM || *first == NEG_PERM_COM) {
		ERR_PRINTF_REPLY(
			reply,
			"Failure: Login failed after sending the password.");
		// TODO: Extract more information from the reply.
		goto fail;
	}
	if (*first != POS_INT) {
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
	if (*first == POS_COM)
		goto succeed;
	if (*first == POS_PRE || *first == POS_INT)
		goto error;
	if (*first == NEG_TRAN_COM || *first == NEG_PERM_COM) {
		ERR_PRINTF_REPLY(
			reply.short_reply,
			"Failure: Login failed after sending the account information.");
		goto fail;
	}
	return 0;
info_needed:
	ERR_PRINTF_REPLY(reply.short_reply,
	                 "%s is needed to log into this server.", info);
	goto fail;
error:
	ERR_PRINTF_REPLY(
		reply.short_reply,
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

enum State { SUCCESS, FAILURE, ERROR };

static inline enum State generic_reply_next_state(struct Reply *reply)
{
	enum ReplyCode1 *first = &reply->first;
	if (*first == POS_COM)
		return SUCCESS;
	if (*first == POS_PRE || *first == POS_INT)
		return ERROR;
	if (*first == NEG_TRAN_COM || *first == NEG_PERM_COM)
		return FAILURE;
	__builtin_unreachable();
}

/**
 *  Suitable for:
 *  ABOR, ALLO, DELE, CWD, CDUP, SMNT, HELP, MODE, NOOP, PASV,
 *  QUIT, SITE, PORT, SYST, STAT, RMD, MKD, PWD, STRU, and TYPE.
 */
static int generic_reply_validate(struct Reply *reply, struct ErrMsg *err,
                                  const char *cmd, const char *desc)
{
	enum State next = generic_reply_next_state(reply);
	if (next == POS_COM)
		return 0;
	if (next == ERROR) {
		ERR_PRINTF_REPLY(
			reply->short_reply,
			"The server shouldn't send this reply to my %s command",
			cmd);
		ERR_WHERE_PRINTF("%s|error", cmd);
		return -1;
	}
	if (next == FAILURE) {
		ERR_PRINTF_REPLY(reply->short_reply, "%s", desc);
		ERR_WHERE_PRINTF("%s|failure", cmd);
		return -1;
	}
	__builtin_unreachable();
}

static int enter_passive_mode(int fd, struct RecvBuf *rb, char *name,
                              char *service, struct ErrMsg *err)
{
	struct Reply reply;
	const char *cmd;
	cmd = "EPSV";
	if (send_command(fd, &reply, err, cmd) < 0)
		return -1;
	if (is_reply_eq(&reply, (unsigned int[]){ 2, 2, 9 })) {
		if (parse_epsv_reply(reply.short_reply, reply.short_reply_len,
		                     service) < 0) {
			ERR_PRINTF("Cannot parse the reply: %s",
			           reply.short_reply);
			ERR_WHERE_PRINTF("EPSV");
			return -1;
		}
		*name = '\0';
		return 0;
	}
	debug("[WARNING] EPSV failed. Falling back to PASV");
	cmd = "PASV";
	if (send_command(fd, &reply, err, cmd) < 0)
		return -1;
	if (generic_reply_validate(&reply, err, cmd,
	                           "Cannot enter passive mode.") < 0)
		return -1;
	if (parse_pasv_reply(reply.short_reply, reply.short_reply_len, name,
	                     service) < 0) {
		ERR_PRINTF("Cannot parse the reply: %s", reply.short_reply);
		ERR_WHERE_PRINTF("PASV");
		return -1;
	}
	return 0;
}

int set_transfer_parameters(int fd, struct RecvBuf *rb, char *name,
                            char *service, struct ErrMsg *err)
{
	if (enter_passive_mode(fd, rb, name, service, err) < 0)
		return -1;

	const char *cmd;
	struct Reply reply;

	// Representation Type: Image
	cmd = "TYPE I";
	if (send_command(fd, &reply, err, cmd) < 0)
		return -1;
	if (generic_reply_validate(
		    &reply, err, cmd,
		    "Cannot set Representation Type to \"Image\".") < 0)
		return -1;

	// File Structure: File
	// Do nothing since File is the default structure.

	// Transfer Mode: Block
	// Do nothing since Stream is the default transfer mode.
	return 0;
}
