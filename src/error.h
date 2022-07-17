#ifndef _ERROR_H
#define _ERROR_H

#include <string.h>
#include <stdio.h>

struct ErrMsg {
#define ERR_MSG_MAX_LEN 256
#define ERR_MSG_WHERE_MAX_LEN 64
	char where[ERR_MSG_WHERE_MAX_LEN];
	char msg[ERR_MSG_MAX_LEN];
};

#define ERR_WHERE()                                                            \
	strcpy(err->where, __func__);                                          \
	_Static_assert(sizeof(__func__) <= ERR_MSG_WHERE_MAX_LEN,              \
	               "Function name too long.");

#define ERR_PRINTF(fmt, ...)                                                   \
	snprintf(err->msg, ERR_MSG_MAX_LEN, fmt, __VA_ARGS__);

#define ERR_WHERE_PRINTF(fmt, ...)                                             \
	snprintf(err->where, ERR_MSG_WHERE_MAX_LEN, fmt, __VA_ARGS__);

#endif
