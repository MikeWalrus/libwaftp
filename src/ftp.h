#ifndef _FTP_H
#define _FTP_H

#include "socket_util.h"

struct UserPI {
	const char *name;
	const char *service;
	struct addrinfo *addr_info;
	int ctrl_fd;
	struct RecvBuf rb;
};

struct ErrMsg;

/// Initialise a \a user_pi
/**
 *  \return NULL on faliure, \a user_pi on success.
 */
struct UserPI *user_pi_init(const char *name, const char *service,
                            struct UserPI *user_pi, struct ErrMsg *err);
#endif
