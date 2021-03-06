#ifndef _CMD_H
#define _CMD_H

#include "telnet.h"

enum ReplyCode1 {
	POS_PRE = 1, /// Positive Preliminary Reply
	POS_COM = 2, /// Positive Completion Reply
	POS_INT = 3, /// Positive Intermediate Reply
	NEG_TRAN_COM = 4, /// Transient Negative Reply
	NEG_PERM_COM = 5, /// Permanent Negative Reply
};

enum ReplyCode2 {
	SYNTAX = 0,
	INFORMATION = 1,
	CONNECTIONS = 2,
	AUTH_ACCOUNTING = 3,
	FILE_SYSTEM = 5
};

struct Reply {
	union {
		struct {
			enum ReplyCode1 first;
			enum ReplyCode2 second;
			unsigned int third;
		};
		unsigned int reply_codes[3];
	};
	char reply[MAX_TELNET_BUF_LEN];
	ssize_t len;
	char short_reply[SHORT_REPLY_MAX_LEN];
	size_t short_reply_len;
};

struct LoginInfo {
	const char *username;
	const char *password;
	const char *account_info;
};

enum ListFormat { FORMAT_LIST, FORMAT_MLSD };

struct UserPI;
struct ErrMsg;
struct RecvBuf;

#define CMD_BUF_LEN 64

/// Send a command and get its primary reply.
/**
 *  /return -1 on error.
 */
int send_command(int fd, struct Reply *reply, struct ErrMsg *err,
                 const char *fmt, ...);

/// Wait for the server to send a bunch of welcome messages and let us send command.
/**
 *  /return -1 on error.
 */
int get_connection_greetings(int fd, struct RecvBuf *rb, struct ErrMsg *err);

int perform_login_sequence(const struct LoginInfo *l, int fd,
                           struct RecvBuf *rb, struct ErrMsg *err);

int set_transfer_parameters(int fd, struct RecvBuf *rb, char *name,
                            char *service, struct ErrMsg *err);

ssize_t list_directory(struct UserPI *user_pi, char *path, char **list,
                       enum ListFormat *format, struct ErrMsg *err);

int download_init(struct UserPI *user_pi, char *path, struct ErrMsg *err);

ssize_t download_chunk(struct UserPI *user_pi, char *data, size_t size,
                       struct ErrMsg *err);

void user_pi_quit(struct UserPI *user_pi);

#endif
