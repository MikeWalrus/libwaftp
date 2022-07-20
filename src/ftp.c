#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "debug.h"

#include "cmd.h"
#include "error.h"
#include "ftp.h"

static int getaddrinfo_ftp(const char *name, const char *service,
                           struct addrinfo **ret)
{
	const struct addrinfo hint =
		(struct addrinfo){ .ai_family = AF_UNSPEC,
		                   .ai_socktype = SOCK_STREAM };
	return getaddrinfo(name, service, &hint, ret);
}

/// Iterate \a addr_info and try to connect.
/**
 *  \return a socket descriptor on success, 0 on faliure.
 */
static int addrinfo_connect(struct addrinfo *addr_info)
{
	for (; addr_info; addr_info = addr_info->ai_next) {
		int s = socket(addr_info->ai_family, addr_info->ai_socktype,
		               addr_info->ai_protocol);
		if (s < 0)
			continue;
		if (connect(s, addr_info->ai_addr, addr_info->ai_addrlen) == 0)
			return s;
		close(s);
	}
	return 0;
}

struct UserPI *user_pi_init(const char *name, const char *service,
                            const struct LoginInfo *login,
                            struct UserPI *user_pi, struct ErrMsg *err)
{
	int n;
	struct addrinfo *addr_info;
	if ((n = getaddrinfo_ftp(name, service, &addr_info)) != 0) {
		ERR_PRINTF("getaddrinfo: %s", gai_strerror(n));
		goto fail;
	}
	int fd = addrinfo_connect(addr_info);
	if (fd <= 0) {
		ERR_PRINTF("Cannot connect to %s, %s", name, service);
		freeaddrinfo(addr_info);
		goto fail;
	}
	*user_pi = (struct UserPI){ .addr_info = addr_info,
		                    .name = name,
		                    .service = service,
		                    .ctrl_fd = fd };
	recv_buf_init(&user_pi->rb);
	if (get_connection_greetings(user_pi->ctrl_fd, &user_pi->rb, err) != 0)
		return NULL;
	if (perform_login_sequence(login, fd, &user_pi->rb, err) != 0)
		return NULL;

	return user_pi;
fail:
	ERR_WHERE();
	return NULL;
}
