#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "parse.h"

#define RETURN_IF_OUT_OF_BOUND()                                               \
	if (ptr >= end)                                                        \
		return -1;

int parse_pasv_reply(const char *reply, size_t len, char *name, char *service)
{
	if (len < 5)
		return -1;
	const char *ptr = reply + 4;
	const char *end = reply + len;

	/// Skip "Entering Passive Mode("
	for (; ptr < end; ptr++) {
		if (isdigit(*ptr))
			break;
	}

	/// "h1,h2,h3,h4,"
	for (size_t i = 0; i < 4; i++) {
		RETURN_IF_OUT_OF_BOUND();
		if (!isdigit(*ptr))
			return -1;
		*(name++) = *(ptr++);
		for (ssize_t j = 0; j < 2; j++) {
			RETURN_IF_OUT_OF_BOUND();
			if (!isdigit(*ptr))
				break;
			*(name++) = *(ptr++);
		}
		RETURN_IF_OUT_OF_BOUND();
		if (*ptr != ',')
			return -1;
		*(name++) = '.';
		ptr++;
	}
	// name = "h1.h2.h3.h4."
	*(name - 1) = '\0';
	// name = "h1.h2.h3.h4\0"

	/// "p1,p2"
	long p1;
	long p2;

	RETURN_IF_OUT_OF_BOUND();
	char *endptr;
	p1 = strtol(ptr, &endptr, 10);
	if (endptr == ptr)
		return -1;
	if (p1 > UINT8_MAX)
		return -1;
	ptr = endptr;
	if (*ptr != ',')
		return -1;
	ptr++;
	p2 = strtol(ptr, &endptr, 10);
	if (endptr == ptr)
		return -1;
	if (p2 > UINT8_MAX)
		return -1;
	uint32_t port = (p1 << 8) + p2;
	sprintf(service, "%d", port);
	return 0;
}

int parse_epsv_reply(const char *reply, size_t len, char *port)
{
	const char *ptr = reply + 4;
	const char *end = reply + len;

	// Find '('
	for (;; ptr++) {
		RETURN_IF_OUT_OF_BOUND();
		if (*ptr == '(')
			break;
	}

	// "<d><d><d>"
	ptr++;
	RETURN_IF_OUT_OF_BOUND();
	const char delimiter = *ptr;
	for (size_t i = 0; i < 2; i++) {
		ptr++;
		RETURN_IF_OUT_OF_BOUND();
		if (*ptr != delimiter)
			return -1;
	}

	// "<tcp-port><d>"
	for (;;) {
		ptr++;
		RETURN_IF_OUT_OF_BOUND();
		if (*ptr == delimiter)
			break;
		*(port++) = *ptr;
	}
	*port = '\0';
	return 0;
}
