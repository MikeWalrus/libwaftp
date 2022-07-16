#include "telnet.h"
#include "socket_util.h"

int send_telnet_negotiation(int fd, unsigned char cmd, unsigned char option)
{
	char cmd_structure[] = { IAC, cmd, option };
	ssize_t n = sendn(fd, cmd_structure, sizeof(cmd_structure));
	if (n <= 0)
		return -1;
	return 0;
}
