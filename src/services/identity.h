/**
 * @file    identity.h
 * @brief   Factory provisioning (PRV1) record reader and field accessors.
 * @details The record sits in bank 2 sector 13 (0x08104000, or 0x08004000
 *          through the bank-swap alias) and is bound to the chip's 96-bit UID.
 *          A blank, torn or foreign record reads as absent.
 *
 *          Header (32 bytes, offset size field):
 *          - 0  4  magic 0x31565250 ("PRV1" in memory)
 *          - 4  2  format, must be 1
 *          - 6  2  provisioning revision
 *          - 8  4  TLV area length, 2..4096 bytes
 *          - 12 4  CRC-32 of the TLV area
 *          - 16 12 chip UID words 0..2 in flash byte order
 *          - 28 4  CRC-32 of header bytes 0..27
 *
 *          TLVs (8-bit type and length, type 0 ends) follow at +32. Read:
 *          serial 0x01, product 0x02, hw revision 0x03, profile 0x06, MAC
 *          0x10; 0x04, 0x05 and 0x07 are skipped.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-04
 * @target  STM32F767ZIT6
 * @srs     identity contract PRV1, factory A-02/A-03/A-05/A-06
 * @ingroup identity
 */

#ifndef IDENTITY_H
#define IDENTITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup identity Factory provisioning identity
 * @brief PRV1 record accessors used to build the HelloReply detail string.
 * @details One cached copy, loaded lazily by the first accessor call. Text
 *          fields are NUL-terminated and truncated to 32 (serial, product),
 *          8 (hw revision) or 64 (profile) characters. Returned strings stay
 *          valid until the next app_identity_refresh().
 * @{
 */

/**
 * @brief Reload and validate the PRV1 record from flash.
 * @details Tries 0x08104000, then the swap alias 0x08004000. Checks magic,
 *          format and length, header CRC, UID, TLV CRC, then non-empty
 *          serial and product. Read-only; never programs flash.
 * @return true if a valid, UID-bound record is now cached.
 * @warning Not thread-safe: rewrites the cache in place. Call before the comms
 *          task serves requests or serialise externally.
 */
bool app_identity_refresh(void);

/**
 * @brief Report whether a valid record is cached, loading it on first use.
 * @return true if a record passed every check.
 * @note A failed load is not retried until app_identity_refresh().
 */
bool app_identity_valid(void);

/**
 * @brief Return the serial number (TLV 0x01).
 * @return NUL-terminated serial, or "" without a valid record.
 */
const char *app_identity_serial(void);

/**
 * @brief Return the product name (TLV 0x02).
 * @return NUL-terminated product name, or "" without a valid record.
 */
const char *app_identity_product(void);

/**
 * @brief Return the hardware revision (optional TLV 0x03).
 * @return NUL-terminated revision, or "" if absent.
 */
const char *app_identity_hw_revision(void);

/**
 * @brief Return the provisioning profile name (optional TLV 0x06).
 * @return NUL-terminated profile, or "" if absent.
 */
const char *app_identity_profile(void);

/**
 * @brief Return the header's provisioning revision (A-02/A-03).
 * @return Revision, or 0 without a valid record.
 * @warning 0 is also a legal revision; check app_identity_valid() first.
 */
uint16_t app_identity_revision(void);

/**
 * @brief Copy the EUI-48 from TLV 0x10, byte for byte.
 * @param[out] mac 6-byte destination; not NULL-checked.
 * @return true if copied; false without a valid record or a 6-byte MAC TLV,
 *         leaving @p mac untouched.
 */
bool app_identity_mac(uint8_t mac[6]);

/**
 * @brief Write the HelloReply identity summary (A-05).
 * @details Format: `PRV1 serial=...;mac=...;hw=...;product=...;profile=...;`
 *          `revision=N`, MAC as AA:BB:..., empty when absent.
 *
 * @param[out] out      Destination buffer.
 * @param[in]  capacity Size of @p out in bytes, including the NUL.
 *
 * @return true if a valid record was summarised.
 * @retval false @p out is NULL, @p capacity is 0, or no record (then @p out
 *               holds "no identity").
 * @note Truncation is silent and still returns true.
 */
bool app_identity_detail(char *out, size_t capacity);

/** @} */

#endif /* IDENTITY_H */
