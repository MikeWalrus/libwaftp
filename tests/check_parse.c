#include <check.h>
#include <stdlib.h>

extern int parse_pasv_reply(const char *reply, size_t len, char *name,
                            char *service);

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

Suite *parse_suite(void)
{
	Suite *s;
	TCase *tc;
	s = suite_create("parse");
	tc = tcase_create("parse_pasv_reply");
	tcase_add_test(tc, test_parse_pasv_reply_valid);
	tcase_add_test(tc, test_parse_pasv_reply_invalid);
	suite_add_tcase(s, tc);
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
