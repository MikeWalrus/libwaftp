#ifndef _FTP_H
#define _FTP_H

#define ERR_MSG_MAX_LEN 256

struct UserPI {
	const char *name;
	const char *service;
	struct addrinfo *addr_info;
	int ctrl_fd;
};

/// Initialise a \a user_pi
/**
 *  \return NULL on faliure, \a user_pi on success.
 */
struct UserPI *user_pi_init(const char *name, const char *service,
                            struct UserPI *user_pi, char *err_msg);
#endif
