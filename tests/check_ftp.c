#include "../src/error.h"
#include "../src/ftp.h"
#include "config.h"

#include <check.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

const char SERVER_IP_V4[] = "127.0.0.1";
const char SERVER_IP_V6[] = "::";
const char SERVER_PORT[] = "12345";
const char SERVER_PROGRAM[] = "pure-ftpd";
const char SERVER_ROOT[] = "server/ftp-root";

const struct LoginInfo anonymous = {
	.username = "anonymous",
	.password = "",
	.account_info = "",
};

static pid_t server_pid;

void exec_server(void)
{
	const size_t buf_len = 1024;
	char cwd[buf_len];
	char program_name[buf_len];
	char ftp_anon_dir[buf_len];
	;
	getcwd(cwd, buf_len);

	snprintf(ftp_anon_dir, buf_len, "FTP_ANON_DIR=%s/%s", cwd, SERVER_ROOT);
	printf("starting: %s\n", program_name);
	const char *const new_argv[] = { SERVER_PROGRAM, "-S", SERVER_PORT,
		                         NULL };
	putenv(ftp_anon_dir);
	execvp(new_argv[0], (char *const *)new_argv);
	perror("execvp:");
	exit(1);
}

void start_server(void)
{
	pid_t pid = fork();
	printf("pid %d\n", pid);
	if (!pid) {
		exec_server();
	} else {
		server_pid = pid;
		sleep(1);
	}
}

void stop_server(void)
{
	if (server_pid > 0) {
		kill(server_pid, 9);
		int wstatus;
		wait(&wstatus);
		assert(WIFSIGNALED(wstatus));
	}
}

struct UserPI user_pi;

void check_user_pi_init_invalid(const char *name, const char *service)
{
	struct ErrMsg err;
	struct UserPI *user_pi_result =
		user_pi_init(name, service, &anonymous, &user_pi, &err);
	ck_assert(user_pi_result != &user_pi);
}

void check_user_pi_init_valid(const char *name, const char *service)
{
	struct ErrMsg err;
	struct UserPI *user_pi_result =
		user_pi_init(name, service, &anonymous, &user_pi, &err);
	ck_assert_msg(user_pi_result == &user_pi, "[%s] %s", err.where,
	              err.msg);
	ck_assert(user_pi.ctrl_fd > 0);
}

void setup(void)
{
}

void teardown(void)
{
}

START_TEST(test_user_pi_init_valid)
{
	check_user_pi_init_valid(SERVER_IP_V4, SERVER_PORT);
	check_user_pi_init_valid(SERVER_IP_V6, SERVER_PORT);
}
END_TEST

START_TEST(test_user_pi_init_invalid)
{
	check_user_pi_init_invalid("&&&&", "ftp");
	check_user_pi_init_invalid(SERVER_IP_V4, "0");
}
END_TEST

Suite *ftp_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("ftp");

	tc = tcase_create("user PI init");

	tcase_add_test(tc, test_user_pi_init_valid);
	tcase_add_test(tc, test_user_pi_init_invalid);

	tcase_set_timeout(tc, 100);
	suite_add_tcase(s, tc);

	return s;
}

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = ftp_suite();
	sr = srunner_create(s);

	start_server();
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	stop_server();
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
