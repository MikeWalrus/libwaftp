#ifndef _FTP_H
#define _FTP_H

#include "cmd.h"
#include "socket_util.h"

struct Connection {
	const char *name;
	const char *service;
	struct addrinfo *addr_info;
	int fd;
};

struct UserPI {
	struct Connection ctrl;
	struct RecvBuf rb;

	struct Connection data;
};

struct ErrMsg;

/// Initialise a \a user_pi
/**
 *  \return NULL on faliure, \a user_pi on success.
 */
struct UserPI *user_pi_init(const char *name, const char *service,
                            const struct LoginInfo *login,
                            struct UserPI *user_pi, struct ErrMsg *err);

int create_data_connection(struct UserPI *user_pi, struct ErrMsg *err);

#endif
