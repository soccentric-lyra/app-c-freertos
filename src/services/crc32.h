/**
 * @file    crc32.h
 * @brief   Table-free CRC-32/ISO-HDLC (zlib crc32()).
 * @details Parameters are fixed by the bootloader contract: poly 0x04C11DB7
 *          (reflected 0xEDB88320), init 0xFFFFFFFF, refin/refout, xorout
 *          0xFFFFFFFF. Used by the HWLINK trailer, the CONFIRMED record and
 *          the identity record.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     HWLINK LNK-003/004, BOOT Appendix B
 * @ingroup crc32
 */

#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup crc32 CRC-32 checksum
 * @brief Table-free CRC-32/ISO-HDLC shared by HWLINK, BOOT metadata and identity.
 * @{
 */

/**
 * @brief Compute CRC-32/ISO-HDLC over a byte buffer.
 * @details Covers bytes in memory order; word arrays depend on little-endian
 *          layout. Pure and reentrant, so task and ISR safe.
 *
 * @param[in] data Bytes to checksum; may be NULL only when @p len is 0.
 * @param[in] len  Length in bytes.
 *
 * @return CRC-32 of the buffer; 0x00000000 when @p len is 0.
 *
 * @note  Check value over "123456789" is 0xCBF43926. Changing any parameter
 *        breaks HWLINK (`LNK-003`, `LNK-004`) and BOOT Appendix B.
 * @warning No NULL check: NULL @p data with non-zero @p len reads address 0.
 * @see hwlink_build_raw, hwlink_decode_raw, app_flash_confirm_running_slot,
 *      app_identity_refresh
 */
uint32_t app_crc32(const void *data, size_t len);

/** @} */

#endif /* CRC32_H */
