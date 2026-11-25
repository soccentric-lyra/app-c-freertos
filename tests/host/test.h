/*
 * test.h - minimal host test harness for the portable application modules
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Target:  host (Linux/macOS) and CI
 *
 * The firmware tests deliberately avoid a test framework dependency: the two
 * modules under test (crc32 and hwlink) are plain C and only need a return
 * code.  Keep the harness small so the tests build with the host compiler in
 * the same image as the firmware.
 */

#ifndef TEST_H
#define TEST_H

#include <stdint.h>
#include <stdio.h>

/** @brief Number of checks that passed. */
extern unsigned test_passed;
/** @brief Number of checks that failed. */
extern unsigned test_failed;

/**
 * @brief Record one boolean check.
 * @param[in] cond  Value to check.
 * @param[in] file  Source file of the check.
 * @param[in] line  Source line of the check.
 * @param[in] expr  Text of the checked expression.
 * @return @p cond, so callers can stop a sequence with `if (!CHECK(...))`.
 */
int test_check(int cond, const char *file, int line, const char *expr);

/**
 * @brief Record one unsigned-integer equality check.
 * @param[in] got   Actual value.
 * @param[in] want  Expected value.
 * @param[in] file  Source file of the check.
 * @param[in] line  Source line of the check.
 * @param[in] expr  Text of the checked expression.
 * @return 1 when equal, 0 otherwise.
 */
int test_check_u32(uint32_t got, uint32_t want, const char *file, int line,
                   const char *expr);

/** @brief Check a condition and return 0 from the test on failure. */
#define CHECK(cond)                                                            \
    do                                                                         \
    {                                                                          \
        if(!test_check((cond) != 0, __FILE__, __LINE__, #cond))                \
        {                                                                      \
            return 0;                                                          \
        }                                                                      \
    } while(0)

/** @brief Compare two unsigned values and return 0 from the test on failure. */
#define CHECK_U32(got, want)                                                   \
    do                                                                         \
    {                                                                          \
        if(!test_check_u32((uint32_t)(got), (uint32_t)(want), __FILE__,       \
                           __LINE__, #got))                                    \
        {                                                                      \
            return 0;                                                          \
        }                                                                      \
    } while(0)

/** @brief Run one test suite; returns 1 on success. */
typedef int (*test_fn)(void);

/** @brief CRC32 test suite. */
int test_crc32(void);
/** @brief HWLINK framing test suite. */
int test_hwlink(void);
/** @brief LED pattern engine test suite. */
int test_led_pattern(void);
/** @brief Button debounce test suite. */
int test_button_logic(void);

#endif /* TEST_H */
