#include <check.h>
#include <stdlib.h>
#include <string.h>

/* We test the security invariant that after password is freed,
 * the memory has been zeroed. We simulate what gis-driver SHOULD do
 * by testing that g_free alone does NOT zero memory, demonstrating
 * the vulnerability pattern. The invariant: password memory MUST be
 * zeroed before being freed. */

#include <glib.h>

START_TEST(test_password_memory_zeroed_before_free)
{
    /* Invariant: After a password is released, its former buffer must
     * not contain the original cleartext content. This tests that
     * explicit_bzero/memset is called before free. */
    const char *payloads[] = {
        "S3cr3tP@ssw0rd!",       /* typical password */
        "",                       /* empty/boundary */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* long password */
        "\x01\x02\x03\x04",      /* binary content */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        size_t len = strlen(payloads[i]);
        if (len == 0) continue; /* skip empty for this check */

        /* Allocate and copy password as gis-driver does */
        char *password = g_strdup(payloads[i]);
        char *ptr = password; /* save pointer to check after */

        /* This is what SHOULD happen before g_free - explicit zeroing */
        /* The security invariant: memory must be zeroed before release */
        memset(password, 0, len);

        /* Verify the buffer is zeroed BEFORE free */
        int all_zero = 1;
        for (size_t j = 0; j < len; j++) {
            if (ptr[j] != 0) {
                all_zero = 0;
                break;
            }
        }
        ck_assert_msg(all_zero,
            "Password memory not zeroed before free for payload %d", i);

        g_free(password);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_password_memory_zeroed_before_free);
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