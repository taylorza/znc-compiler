#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* Invariant: compiler.c must not overflow outfilename buffer regardless of input size.
   We invoke the compiler process with adversarial filenames and assert it does not
   crash with a signal (SIGSEGV/SIGABRT) indicating a buffer overflow. */

START_TEST(test_no_buffer_overflow_on_oversized_input)
{
    /* Payloads: exact exploit (256 bytes), boundary (128 bytes), valid short name */
    const char *payloads[] = {
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  /* 256 A's */
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",  /* 128 B's */
        "valid_input.src"
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        /* Write a minimal source file with the oversized token as output filename */
        char tmpfile[] = "/tmp/test_compiler_XXXXXX.src";
        int fd = mkstemps(tmpfile, 4);
        ck_assert_int_ge(fd, 0);

        /* Write a source that triggers the outfilename strcpy path */
        dprintf(fd, "output %s\n", payloads[i]);
        close(fd);

        pid_t pid = fork();
        ck_assert_int_ge(pid, 0);

        if (pid == 0) {
            /* Child: exec the real compiler with the crafted input */
            execl("./compiler", "compiler", tmpfile, NULL);
            _exit(127); /* exec failed */
        } else {
            int status = 0;
            waitpid(pid, &status, 0);
            unlink(tmpfile);

            if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                /* SIGSEGV=11, SIGABRT=6 indicate buffer overflow / memory corruption */
                ck_assert_msg(sig != 11 && sig != 6,
                    "Compiler crashed with signal %d on payload[%d] (buffer overflow)", sig, i);
            }
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 10);

    tcase_add_test(tc_core, test_no_buffer_overflow_on_oversized_input);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}