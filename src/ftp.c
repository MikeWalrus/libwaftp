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

static int try_connect(const char *name, const char *service, int *fd,
                       struct addrinfo **ai, struct ErrMsg *err)
{
	int n;
	if ((n = getaddrinfo_ftp(name, service, ai)) != 0) {
		ERR_PRINTF("getaddrinfo: %s", gai_strerror(n));
		return -1;
	}
	*fd = addrinfo_connect(*ai);
	if (*fd <= 0) {
		ERR_PRINTF("Cannot connect to %s, %s", name, service);
		freeaddrinfo(*ai);
		return -1;
	}
	return 0;
}

static int data_connection_connect(struct Connection *data_con,
                                   const char *ctrl_name, const char *name,
                                   const char *service, struct ErrMsg *err)
{
	if (!*name)
		name = ctrl_name;
	if (try_connect(name, service, &data_con->fd, &data_con->addr_info,
	                err) < 0) {
		ERR_WHERE_PRINTF("Data Connection");
		return -1;
	}
	freeaddrinfo(data_con->addr_info);
	debug("[INFO] Data connection established.\n");
	return 0;
}

int create_data_connection(struct UserPI *user_pi, struct ErrMsg *err)
{
	char name_data[3 * 4 + 3 + 1];
	char service_data[7];
	if (set_transfer_parameters(user_pi->ctrl.fd, &user_pi->rb, name_data,
	                            service_data, err) != 0)
		return -1;
	if (data_connection_connect(&user_pi->data, user_pi->ctrl.name,
	                            name_data, service_data, err) < 0)
		return -1;
	return 0;
}

struct UserPI *user_pi_init(const char *name, const char *service,
                            const struct LoginInfo *login,
                            struct UserPI *user_pi, struct ErrMsg *err)
{
	int ctrl_fd;
	struct addrinfo *ctrl_ai;
	if (try_connect(name, service, &ctrl_fd, &ctrl_ai, err) < 0) {
		ERR_WHERE_PRINTF("Control Connection");
		return NULL;
	}
	debug("[INFO] Control Connection established.\n");
	user_pi->ctrl = (struct Connection){ .addr_info = ctrl_ai,
		                             .name = name,
		                             .service = service,
		                             .fd = ctrl_fd };
	recv_buf_init(&user_pi->rb);
	if (get_connection_greetings(user_pi->ctrl.fd, &user_pi->rb, err) != 0)
		return NULL;
	if (perform_login_sequence(login, user_pi->ctrl.fd, &user_pi->rb,
	                           err) != 0)
		return NULL;

	return user_pi;
}

// addr_info still belongs to \a src
int user_pi_clone(const struct UserPI *src, struct UserPI *dest,
                  const struct LoginInfo *login, struct ErrMsg *err)
{
	*dest = (struct UserPI){ .ctrl.addr_info = src->ctrl.addr_info,
		                 .ctrl.name = src->ctrl.name };
	int fd = addrinfo_connect(dest->ctrl.addr_info);
	if (fd <= 0) {
		ERR_PRINTF("Cannot connect to the server.");
		ERR_WHERE();
	}
	dest->ctrl.fd = fd;
	recv_buf_init(&dest->rb);
	if (get_connection_greetings(dest->ctrl.fd, &dest->rb, err) != 0)
		return -1;
	if (perform_login_sequence(login, dest->ctrl.fd, &dest->rb, err) != 0)
		return -1;
	return 0;
}

void user_pi_drop(struct UserPI *user_pi)
{
	user_pi_quit(user_pi);
	freeaddrinfo(user_pi->ctrl.addr_info);
}
