#ifndef _FTP_H
#define _FTP_H

#include "socket_util.h"

struct ErrMsg {
#define ERR_MSG_MAX_LEN 256
#define ERR_MSG_WHERE_MAX_LEN 64
	char where[ERR_MSG_WHERE_MAX_LEN];
	char msg[ERR_MSG_MAX_LEN];
};

struct UserPI {
	const char *name;
	const char *service;
	struct addrinfo *addr_info;
	int ctrl_fd;
	struct RecvBuf rb;
};

/// Initialise a \a user_pi
/**
 *  \return NULL on faliure, \a user_pi on success.
 */
struct UserPI *user_pi_init(const char *name, const char *service,
                            struct UserPI *user_pi, struct ErrMsg *err);
#endif
