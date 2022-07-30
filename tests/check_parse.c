#include "../src/parse.h"

#include <check.h>
#include <stdlib.h>
#include <time.h>

void parse_pasv_reply_check_valid(const char *reply, char *name, char *service)
{
	char name_got[3 * 4 + 3 + 1];
	char service_got[6];
	int ret = parse_pasv_reply(reply, strlen(reply), name_got, service_got);
	ck_assert_msg(ret == 0, "error when parsing reply: %s\n", reply);
	ck_assert_str_eq(name_got, name);
	ck_assert_str_eq(service_got, service);
}

void parse_pasv_reply_check_invalid(const char *reply)
{
	char name_got[3 * 4 + 3 + 1];
	char service_got[6];
	int ret = parse_pasv_reply(reply, strlen(reply), name_got, service_got);
	ck_assert_msg(ret < 0, "should fail reply: %s\n", reply);
}

START_TEST(test_parse_pasv_reply_valid)
{
	parse_pasv_reply_check_valid(
		"227 Entering Passive Mode (192,168,1,1,0,123)", "192.168.1.1",
		"123");
	parse_pasv_reply_check_valid(
		"227 Entering Passive Mode (1,1,1,1,12,123)", "1.1.1.1",
		"3195");
	parse_pasv_reply_check_valid(
		"227 Entering Passive Mode (255,255,255,255,1,0)",
		"255.255.255.255", "256");
}
END_TEST

START_TEST(test_parse_pasv_reply_invalid)
{
	const char *replies[] = {
		"aaa",
		"227",
		"227 ",
		"227 Entering",
		"227 Entering Passive Mode (255.255,255,255,1,0)",
		"227 Entering Passive Mode (2555,255,255,255,1,0)",
		"227 Entering Passive Mode (255,255,255,255,257,0)",
		"227 Entering Passive Mode (255,255,255,255,0,257)",
		"227 Entering Passive Mode (255,255,255,255,1234567890,0)",
		"227 Entering Passive Mode (255,,255,255,0,0)"
	};
	for (size_t i = 0; i < sizeof(replies) / sizeof(replies[0]); i++) {
		parse_pasv_reply_check_invalid(replies[i]);
	}
}
END_TEST

void parse_epsv_reply_check_valid(const char *reply, const char *port)
{
	char port_got[6];
	int ret = parse_epsv_reply(reply, strlen(reply), port_got);
	ck_assert_msg(ret == 0, "error when parsing reply: %s\n", reply);
	ck_assert_str_eq(port_got, port);
}

void parse_epsv_reply_check_invalid(const char *reply)
{
	char port_got[6];
	int ret = parse_epsv_reply(reply, strlen(reply), port_got);
	ck_assert_msg(ret < 0, "should fail reply: %s\n", reply);
}

START_TEST(test_parse_epsv_reply_valid)
{
	parse_epsv_reply_check_valid(
		"229 Entering Extended Passive Mode (|||6446|))", "6446");
	parse_epsv_reply_check_valid(
		"229 Entering Extended Passive Mode (||||))", "");
	parse_epsv_reply_check_valid(
		"229 Entering Extended Passive Mode (|||1|))", "1");
	parse_epsv_reply_check_valid(
		"229 Entering Extended Passive Mode (|||12|))", "12");
}
END_TEST

START_TEST(test_parse_epsv_reply_invalid)
{
	const char *replies[] = {
		"229", "229 ||||", "229 (|||)",
		"229 Entering Extended Passive Mode (|1||1234|))"
	};
	for (size_t i = 0; i < sizeof(replies) / sizeof(replies[0]); i++) {
		parse_epsv_reply_check_invalid(replies[i]);
	}
}
END_TEST

void parse_line_list_gnu_check_valid(const char *line, struct Fact fact_ref,
                                     struct tm tm_ref)
{
	bool ignore = true;
	const char *ptr = line;
	struct Fact fact;
	ck_assert(parse_line_list_gnu(line, &ignore, &ptr, &fact) == 0);
	ck_assert(!ignore);
	ck_assert_int_eq(fact.is_dir, fact_ref.is_dir);
	ck_assert_str_eq(fact.name, fact_ref.name);
	ck_assert_int_eq(fact.size, fact_ref.size);

	struct tm modify_tm;
	gmtime_r(&fact.modify, &modify_tm);
	if (timegm(&tm_ref) != fact.modify) {
		ck_assert_int_eq(modify_tm.tm_year, tm_ref.tm_year);
		ck_assert_int_eq(modify_tm.tm_mon, tm_ref.tm_mon);
		ck_assert_int_eq(modify_tm.tm_min, tm_ref.tm_min);
		ck_assert_int_eq(modify_tm.tm_hour, tm_ref.tm_hour);
		ck_assert_int_eq(modify_tm.tm_mday, tm_ref.tm_mday);
	}
}

START_TEST(test_parse_list_gnu_reply_valid)
{
	parse_line_list_gnu_check_valid(
		"-rw-r--r--    1 0        0           17864 Oct 23  2003 MISSING-FILES\r\n",
		(struct Fact){ .is_dir = false,
	                       .name = "MISSING-FILES",
	                       .perm = "rw-r--r--",
	                       .size = 17864 },
		(struct tm){ .tm_year = 2003 - 1900,
	                     .tm_mon = 9,
	                     .tm_mday = 23,
	                     .tm_hour = 0,
	                     .tm_min = 0 });
	parse_line_list_gnu_check_valid(
		"lrwxrwxrwx    1 0        0               8 Aug 20  2004 CRYPTO.README -> .message\r\n",
		(struct Fact){
			.is_dir = false,
			.name = "CRYPTO.README",
			.perm = "rwxrwxrwx",
			.size = -1 }, // We don't know the size since it's a link.
		(struct tm){ .tm_year = 2004 - 1900,
	                     .tm_mon = 7,
	                     .tm_mday = 20,
	                     .tm_hour = 0,
	                     .tm_min = 0 });
	parse_line_list_gnu_check_valid( // NOTE: This test case is time-dependent.
		"-rw-rw-r--    1 0        3003       465860 Jul 29 21:09 ls-lrRt.txt.gz\r\n",
		(struct Fact){ .is_dir = false,
	                       .name = "ls-lrRt.txt.gz",
	                       .perm = "rw-rw-r--",
	                       .size = 465860 },
		(struct tm){ .tm_year = 2022 - 1900,
	                     .tm_mon = 6,
	                     .tm_mday = 29,
	                     .tm_hour = 21,
	                     .tm_min = 9 });
	parse_line_list_gnu_check_valid(
		"drwxr-xr-x    2 0        0            4096 Apr 07  2009 tmp\r\n",
		(struct Fact){ .is_dir = true,
	                       .name = "tmp",
	                       .perm = "drwxr-xr-x",
	                       .size = 4096 },
		(struct tm){ .tm_year = 2009 - 1900,
	                     .tm_mon = 3,
	                     .tm_mday = 7,
	                     .tm_hour = 0,
	                     .tm_min = 0 });
}
END_TEST

Suite *parse_suite(void)
{
	Suite *s;
	s = suite_create("parse");
	TCase *pasv_tc = tcase_create("parse_pasv_reply");
	tcase_add_test(pasv_tc, test_parse_pasv_reply_valid);
	tcase_add_test(pasv_tc, test_parse_pasv_reply_invalid);
	suite_add_tcase(s, pasv_tc);
	TCase *epsv_tc = tcase_create("parse_epsv_reply");
	tcase_add_test(epsv_tc, test_parse_epsv_reply_valid);
	tcase_add_test(epsv_tc, test_parse_epsv_reply_invalid);
	suite_add_tcase(s, epsv_tc);
	TCase *list_gnu_tc = tcase_create("parse_list_gnu_reply");
	tcase_add_test(list_gnu_tc, test_parse_list_gnu_reply_valid);
	suite_add_tcase(s, list_gnu_tc);
	return s;
}

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = parse_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
