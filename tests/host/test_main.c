/*
 * test_main.c - run the portable host test suites
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Target:  host (Linux/macOS) and CI
 */

#include "test.h"

unsigned test_passed;
unsigned test_failed;

int test_check(int cond, const char *file, int line, const char *expr)
{
    if(cond)
    {
        test_passed++;
        return 1;
    }
    test_failed++;
    (void)fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
    return 0;
}

int test_check_u32(uint32_t got, uint32_t want, const char *file, int line,
                   const char *expr)
{
    if(got == want)
    {
        test_passed++;
        return 1;
    }
    test_failed++;
    (void)fprintf(stderr, "FAIL %s:%d: %s: got 0x%08X, want 0x%08X\n", file,
                  line, expr, (unsigned)got, (unsigned)want);
    return 0;
}

int main(void)
{
    static const struct
    {
        const char *name;
        test_fn     fn;
    } suites[] = {
        {"crc32", test_crc32},
        {"hwlink", test_hwlink},
        {"led_pattern", test_led_pattern},
        {"button_logic", test_button_logic},
    };

    for(size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++)
    {
        unsigned before = test_failed;
        int      ok     = suites[i].fn();
        if(ok && test_failed == before)
        {
            (void)printf("ok   %s\n", suites[i].name);
        }
        else
        {
            (void)printf("FAIL %s\n", suites[i].name);
        }
    }

    (void)printf("%u checks, %u failed\n", test_passed + test_failed,
                 test_failed);
    return test_failed == 0 ? 0 : 1;
}
