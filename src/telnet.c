#include <unistd.h>

#include "debug.h"

#include "cmd.h"
#include "socket_util.h"
#include "telnet.h"

/**
 *  \return On error, returns -1 and sets errno.
 */
static int send_telnet_negotiation(int fd, unsigned char cmd, unsigned char option)
{
	char cmd_structure[] = { IAC, cmd, option };
	ssize_t n = sendn(fd, cmd_structure, sizeof(cmd_structure));
	if (n <= 0)
		return -1;
	return 0;
}

int copy_from_telnet_line(int fd, const unsigned char *line, size_t len,
                          struct Reply *reply)
{
#define INC_OR_RETURN(p)                                                       \
	p++;                                                                   \
	if (p >= end) {                                                        \
		goto end;                                                      \
	}

#define TELNET_CMD()                                                           \
	INC_OR_RETURN(ptr);                                                    \
	unsigned char cmd = *ptr;                                              \
	if (cmd == DONT || cmd == WONT || cmd == DO || cmd == WILL) {          \
		INC_OR_RETURN(ptr);                                            \
		unsigned char opt = *ptr;                                      \
		unsigned char reply = 0;                                       \
		if (cmd == WILL)                                               \
			reply = DONT;                                          \
		if (cmd == DO)                                                 \
			reply = WONT;                                          \
		if (reply == 0)                                                \
			continue;                                              \
		if (send_telnet_negotiation(fd, reply, opt) < 0)               \
			return -1;                                             \
	}

	const unsigned char *ptr = line;
	const unsigned char *end = line + len;

	char *dest = &reply->reply[reply->len];
	char *dest_end = dest + MAX_TELNET_BUF_LEN;

	for (size_t i = 0; i < SHORT_REPLY_MAX_LEN; i++) {
		unsigned char c = *ptr;
		if (c != IAC) {
			reply->short_reply[i] = c;
			if (dest < dest_end)
				*(dest++) = c;
		} else {
			TELNET_CMD();
		}
		ptr++;
		if (ptr >= end) {
			reply->short_reply_len = i;
			goto end;
		}
	}
	reply->short_reply_len = SHORT_REPLY_MAX_LEN;

	for (; ptr < end; ptr++) {
		unsigned char c = *ptr;
		if (c != IAC) {
			if (dest < dest_end)
				*(dest++) = c;
			continue;
		}
		TELNET_CMD();
	}
end:
	reply->len = dest - reply->reply;
	return 0;
}
