#ifndef _TELNET_H
#define _TELNET_H

#include <arpa/telnet.h>
#include <stdbool.h>
#include <sys/types.h>

#define MAX_TELNET_BUF_LEN 1024

struct Reply;
#define SHORT_REPLY_MAX_LEN 128

/**
 *  Responds to the Telnet commands in \a line,
 *  copies the first SHORT_REPLY_MAX_LEN-1 non-command characters
 *  to `reply->short_reply`, and appends the line to `reply->reply`.
 */
int copy_from_telnet_line(int fd, const unsigned char *line, size_t len,
                          struct Reply *reply);

#endif
