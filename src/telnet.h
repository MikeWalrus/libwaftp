#ifndef _TELNET_H
#define _TELNET_H

#include <arpa/telnet.h>

/**
 *  \return On error, returns -1 and sets errno.
 */
int send_telnet_negotiation(int fd, unsigned char cmd, unsigned char option);

/// Tries to read telnet commands and send negative acknowledgements.
/**
 *  If the current character is NOT IAC, does nothing.
 *  Otherwise, consume telnet commands greedily until `*ptr == end`.
 *  \return On error, returns -1 and sets errno.
 */
static inline int try_read_telnet_cmd_and_reply(unsigned char **ptr,
                                                const unsigned char *const end,
                                                int fd)
{
#define inc_return_if_out_of_boundary(p)                                       \
	(*p)++;                                                                \
	if (*(p) == end)                                                       \
		return 0;

	for (;;) {
		if (**ptr != IAC)
			return 0;
		inc_return_if_out_of_boundary(ptr);
		unsigned char cmd = **ptr;
		if (cmd == DONT || cmd == WONT || cmd == DO || cmd == WILL) {
			inc_return_if_out_of_boundary(ptr);
			unsigned char opt = **ptr;
			unsigned char reply = 0;
			if (cmd == WILL)
				reply = DONT;
			if (cmd == DO)
				reply = WONT;
			if (reply == 0)
				continue;
			if (send_telnet_negotiation(fd, reply, opt) < 0)
				return -1;
		}
		if (cmd == IAC) {
			// This is an escaped IAC.
			return 0;
		}
		inc_return_if_out_of_boundary(ptr);
	}
}

#endif
