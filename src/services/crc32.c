/**
 * @file    crc32.c
 * @brief   Bit-serial CRC-32/ISO-HDLC implementation.
 * @details Computed from the reflected polynomial instead of a 1 KiB table,
 *          trading cycles for RAM. Parameters are fixed by the bootloader
 *          contract; see @ref crc32.h.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6 (NUCLEO-F767ZI)
 * @srs     HWLINK LNK-003/004, BOOT Appendix B
 * @ingroup crc32
 */

#include "services/crc32.h"

/**
 * @brief Compute CRC-32/ISO-HDLC over a byte buffer.
 * @details About eight shift/XOR steps per byte; fine for frames and records,
 *          not for bulk image checks. Callers reading flash through the
 *          D-cache must invalidate the range first.
 *
 * @param[in] data Bytes to checksum; may be NULL only when @p len is 0.
 * @param[in] len  Length in bytes.
 *
 * @return CRC-32 of the buffer; 0x00000000 when @p len is 0.
 *
 * @warning Do not change the constants: doing so breaks HWLINK (`LNK-003`,
 *          `LNK-004`), stored CONFIRMED records and fielded identity records.
 */
uint32_t app_crc32(const void *data, size_t len)
{
    const uint8_t *p   = (const uint8_t *)data;
    uint32_t       crc = 0xFFFFFFFFUL;

    for(size_t i = 0U; i < len; i++)
    {
        crc ^= p[i];
        for(uint32_t bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xEDB88320UL : crc >> 1U;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}
