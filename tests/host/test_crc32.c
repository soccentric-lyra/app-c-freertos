/*
 * test_crc32.c - CRC-32/ISO-HDLC test vectors
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Target:  host (Linux/macOS) and CI
 *
 * The check value 0xCBF43926 for "123456789" is the standard CRC-32/ISO-HDLC
 * vector and is also what zlib.crc32 returns.  A regression here breaks
 * HWLINK and the bootloader's stored records, so it is checked first.
 */

#include "services/crc32.h"
#include "test.h"

int test_crc32(void)
{
    static const uint8_t digits[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

    CHECK_U32(app_crc32(digits, sizeof(digits)), 0xCBF43926UL);
    CHECK_U32(app_crc32(NULL, 0U), 0U);
    CHECK_U32(app_crc32((const uint8_t *)"", 0U), 0U);

    /* One byte, two bytes and an embedded zero must all agree with zlib. */
    CHECK_U32(app_crc32((const uint8_t *)"a", 1U), 0xE8B7BE43UL);
    CHECK_U32(app_crc32((const uint8_t *)"ab", 2U), 0x9E83486DUL);
    CHECK_U32(app_crc32((const uint8_t *)"\x00", 1U), 0xD202EF8DUL);

    /* A buffer is the same as the CRC of its prefix chained through the
     * one-shot API only when the prefix is re-seeded, so just pin a longer
     * vector to catch accidental table or polynomial changes. */
    CHECK_U32(app_crc32((const uint8_t *)"The quick brown fox jumps over the lazy dog", 43U),
              0x414FA339UL);

    return 1;
}
