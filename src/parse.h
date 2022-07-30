#ifndef _PARSE_H
#define _PARSE_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

int parse_pasv_reply(const char *reply, size_t len, char *name, char *service);

int parse_epsv_reply(const char *reply, size_t len, char *port);

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
