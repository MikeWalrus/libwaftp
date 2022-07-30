/* Public header for libwaftp */
#ifndef _LIBWAFTP_H
#define _LIBWAFTP_H

#include <stdbool.h>

#include <sys/types.h>

#define LINE_MAX_LEN 1024
struct RecvBuf {
	char buf[LINE_MAX_LEN];
	char *read_ptr;
	int remain_count;
};

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

struct ErrMsg {
#define ERR_MSG_MAX_LEN 256
#define ERR_MSG_WHERE_MAX_LEN 64
	char where[ERR_MSG_WHERE_MAX_LEN];
	char msg[ERR_MSG_MAX_LEN];
};

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

enum ListFormat { FORMAT_LIST, FORMAT_MLSD };

ssize_t list_directory(struct UserPI *user_pi, char *path, char **list,
                       enum ListFormat *format, struct ErrMsg *err);

struct Fact {
	char *name;
	bool is_dir;
	ssize_t size;
#define FACT_PERM_MAX_LEN 64
	char perm[FACT_PERM_MAX_LEN];
	time_t modify;
};

int parse_line_list_gnu(const char *list, bool *ignore, const char **end,
                        struct Fact *fact);

#endif
