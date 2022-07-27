/* Public header for libwaftp */
#ifndef _LIBWAFTP_H
#define _LIBWAFTP_H

#include <sys/types.h>

struct UserPI;
struct ErrMsg;

struct LoginInfo {
	const char *username;
	const char *password;
	const char *account_info;
};

/// Initialise a \a user_pi
/**
 *  \return NULL on faliure, \a user_pi on success.
 */
struct UserPI *user_pi_init(const char *name, const char *service,
                            const struct LoginInfo *login,
                            struct UserPI *user_pi, struct ErrMsg *err);

ssize_t list_directory(struct UserPI *user_pi, char *path, char **list,
                       struct ErrMsg *err);

#endif
