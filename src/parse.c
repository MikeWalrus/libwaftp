#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"
#include "parse.h"

#define RETURN_IF_OUT_OF_BOUND()                                               \
	if (ptr >= end)                                                        \
		return -1;

#define RETURN_IF_0()                                                          \
	if (!*ptr)                                                             \
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

static int copy_until(const char *begin, const char **end, char *dest, char c,
                      size_t len)
{
	const char *ptr = begin;
	for (size_t i = 0; i < len - 1; i++) {
		RETURN_IF_0();
		*(dest++) = *(ptr++);
		if (*ptr == c) {
			*(dest++) = '\0';
			*end = ptr;
			return 0;
		}
	}
	return -1;
}

static int skip_at_least_one_eq(const char *begin, const char **end, char c)
{
	const char *ptr = begin;
	RETURN_IF_0();

	for (ptr++;; ptr++) {
		if (*ptr == '\0' || *ptr != c) {
			*end = ptr;
			return 0;
		}
	}
}

static int skip_at_least_one_neq(const char *begin, const char **end, char c)
{
	const char *ptr = begin;
	RETURN_IF_0();

	for (ptr++;; ptr++) {
		if (*ptr == '\0' || *ptr == c) {
			*end = ptr;
			return 0;
		}
	}
}

static int skip_at_least_one_f(const char *begin, const char **end,
                               int (*f)(int))
{
	const char *ptr = begin;
	RETURN_IF_0();

	for (ptr++;; ptr++) {
		if (*ptr == '\0' || !f(*ptr)) {
			*end = ptr;
			return 0;
		}
	}
}

static int skip_at_least_one_f_neg(const char *begin, const char **end,
                                   int (*f)(int))
{
	const char *ptr = begin;
	RETURN_IF_0();

	for (ptr++;; ptr++) {
		if (*ptr == '\0' || f(*ptr)) {
			*end = ptr;
			return 0;
		}
	}
}

const static char *const months[12] = { "Jan", "Feb", "Mar", "Apr",
	                                "May", "Jun", "Jul", "Aug",
	                                "Sep", "Oct", "Nov", "Dec" };

/**
 *  \returns -1 on error.
 */
static int get_mon(char *str)
{
	for (size_t i = 0; i < 12; i++) {
		if (!strcmp(months[i], str))
			return i;
	}
	return -1;
}

#define PARSE_FUNC(type, TYPE, min)                                            \
	static int parse_##type(const char *begin, const char **end,           \
	                        type *value)                                   \
	{                                                                      \
		if (!*begin)                                                   \
			return -1;                                             \
		char *strtol_end;                                              \
		long long num = strtoll(begin, &strtol_end, 10);               \
		if (strtol_end == begin)                                       \
			return -1;                                             \
		*end = strtol_end;                                             \
		if (num < (min) || num > TYPE##_MAX)                           \
			return -1;                                             \
		*value = num;                                                  \
		return 0;                                                      \
	}

PARSE_FUNC(int, INT, INT_MIN)
PARSE_FUNC(ssize_t, SSIZE, -1)

static int guess_year(int mon, int mday, int hour, int min)
{
#define CUTOFF_MONTH 6
	time_t now = time(NULL);
	struct tm now_tm;
	gmtime_r(&now, &now_tm);
	struct tm cutoff_tm = now_tm;
	cutoff_tm.tm_mon -= CUTOFF_MONTH;
	time_t cutoff = timegm(&cutoff_tm);
	struct tm guess_tm = { .tm_year = now_tm.tm_year,
		               .tm_mon = mon,
		               .tm_mday = mday,
		               .tm_hour = hour,
		               .tm_min = min };
	time_t guess = timegm(&guess_tm);
	if (guess > cutoff && guess < now)
		return now_tm.tm_year;
	return now_tm.tm_year - 1;
}

int parse_line_list_gnu(const char *list, bool *ignore, const char **end,
                        struct Fact *fact)
{
	*ignore = false;
	const char *ptr = list;

	// is_dir
	RETURN_IF_0();
	fact->is_dir = *ptr == 'd';
	bool is_link = *ptr == 'l';
	ptr++;

	// perm
	if (copy_until(ptr, &ptr, fact->perm, ' ', FACT_PERM_MAX_LEN) < 0)
		return -1;
	if (skip_at_least_one_eq(ptr, &ptr, ' ') < 0)
		return -1;

	// nlink owner group
	for (size_t i = 0; i < 3; i++) {
		if (skip_at_least_one_neq(ptr, &ptr, ' ') < 0)
			return -1;
		if (skip_at_least_one_eq(ptr, &ptr, ' ') < 0)
			return -1;
	}

	// size
	char *strtol_end;
	if (parse_ssize_t(ptr, &ptr, &fact->size) < 0)
		return -1;
	if (skip_at_least_one_eq(ptr, &ptr, ' ') < 0)
		return -1;
	if (is_link)
		fact->size = -1;

	// Month
	const size_t mon_str_len = 4;
	char mon_str[mon_str_len];
	if (copy_until(ptr, &ptr, mon_str, ' ', mon_str_len) < 0)
		return -1;
	int mon = get_mon(mon_str);
	if (mon < 0)
		return -1;
	if (skip_at_least_one_eq(ptr, &ptr, ' ') < 0)
		return -1;

	// Day
	int mday;
	if (parse_int(ptr, &ptr, &mday) < 0)
		return -1;
	if (skip_at_least_one_eq(ptr, &ptr, ' ') < 0)
		return -1;

	// Year or Time
	int year;
	int hour = 0;
	int min = 0;
	for (size_t i = 0; i < 3; i++) {
		if (!ptr[i])
			return -1;
	}
	bool is_year = !(ptr[2] == ':');
	if (is_year) {
		int year_abs;
		if (parse_int(ptr, &ptr, &year_abs) < 0)
			return -1;
		year = year_abs - 1900;
	} else {
		if (parse_int(ptr, &ptr, &hour) < 0)
			return -1;
		assert(*ptr == ':');
		ptr++;
		if (parse_int(ptr, &ptr, &min) < 0)
			return -1;
		year = guess_year(mon, mday, hour, min);
	}

	struct tm modify_tm = { .tm_year = year,
		                .tm_mon = mon,
		                .tm_mday = mday,
		                .tm_hour = hour,
		                .tm_min = min };
	fact->modify = timegm(&modify_tm);
	if (skip_at_least_one_eq(ptr, &ptr, ' ') < 0)
		return -1;
	const char *name = ptr;
	if (skip_at_least_one_f_neg(ptr, &ptr, isspace) < 0)
		return -1;
	if (ptr <= name)
		return -1;
	size_t name_len = ptr - name;
	fact->name = malloc(name_len + 1);
	for (size_t i = 0; i < name_len; i++)
		fact->name[i] = name[i];
	fact->name[name_len] = '\0';

	// optional -> ... and mandatory CRLF
	skip_at_least_one_neq(ptr, &ptr, '\n');
	RETURN_IF_0();

	*end = ptr + 1;
	return 0;
}

int parse_line_mlsd(const char *list, bool *ignore, const char **end,
                    struct Fact *fact)
{
	return -1;
}
