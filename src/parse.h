#ifndef _PARSE_H
#define _PARSE_H

#include <stdint.h>
#include <sys/types.h>

int parse_pasv_reply(const char *reply, size_t len, char *name, char *service);

int parse_epsv_reply(const char *reply, size_t len, char *port);

#endif
