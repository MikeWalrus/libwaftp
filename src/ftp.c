#include "ftp.h"
#include "config.h"

#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
int addrinfo_connect(struct addrinfo *addr_info)
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
                            struct UserPI *user_pi, char *err_msg)
{
	int n;
	struct addrinfo *addr_info;
	if ((n = getaddrinfo_ftp(name, service, &addr_info)) != 0) {
		snprintf(err_msg, ERR_MSG_MAX_LEN, "getaddrinfo: %s",
		         gai_strerror(n));
		return NULL;
	}
	int fd = addrinfo_connect(addr_info);
	if (fd <= 0) {
		snprintf(err_msg, ERR_MSG_MAX_LEN,
		         "user_pi_init: Cannot connect to %s, %s", name,
		         service);
		return NULL;
	}
	*user_pi = (struct UserPI){ .addr_info = addr_info,
		                    .name = name,
		                    .service = service,
		                    .ctrl_fd = fd };
	return user_pi;
}
