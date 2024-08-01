/**
 * @file    identity.c
 * @brief   Factory provisioning record reader (A-03).
 * @details Validation stops at the first failure: magic, format and length,
 *          header CRC, UID match, TLV CRC, non-empty serial and product.
 *          Layout is in @ref identity.h.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-04
 * @target  STM32F767ZIT6
 * @srs     identity contract PRV1, factory A-02/A-03/A-05/A-06
 * @ingroup identity
 */

#include <string.h>
#include "stm32f7xx_hal.h"
#include "services/crc32.h"
#include "services/identity.h"

/** @brief PRV1 magic; memory bytes read "PRV1". */
#define ID_MAGIC 0x31565250UL /* "PRV1" little-endian */

/** @brief Only accepted record format. */
#define ID_FORMAT 1U

/** @brief Header size in bytes; the TLV area follows it. */
#define ID_HEADER_SIZE 32U

/** @brief Maximum TLV area length in bytes. */
#define ID_MAX_TLV 4096U

#define ID_T_SERIAL           0x01U /**< Serial number TLV. */
#define ID_T_PRODUCT          0x02U /**< Product name TLV. */
#define ID_T_HW_REV           0x03U /**< Hardware revision TLV. */
#define ID_T_MFG_TIME         0x04U /**< Manufacturing time TLV; skipped. */
#define ID_T_STATION_FIXTURE  0x05U /**< Station/fixture TLV; skipped. */
#define ID_T_PROFILE          0x06U /**< Provisioning profile TLV. */
#define ID_T_FIRMWARE_VERSION 0x07U /**< Firmware version TLV; skipped. */
#define ID_T_MAC              0x10U /**< EUI-48 TLV; accepted only if 6 bytes. */

/** @brief Primary record address, bank 2 sector 13. */
#define ID_IDENTITY_A 0x08104000UL

/** @brief Record address while bank 2 is swapped to 0x08000000. */
#define ID_IDENTITY_SWAPPED 0x08004000UL

/**
 * @brief PRV1 header as stored in flash.
 * @note Field order and natural alignment give the 32-byte flash layout.
 */
typedef struct
{
    uint32_t magic;        /**< @ref ID_MAGIC when present. */
    uint16_t format;       /**< Must equal @ref ID_FORMAT. */
    uint16_t revision;     /**< Provisioning revision. */
    uint32_t length;       /**< TLV area length, 2..@ref ID_MAX_TLV bytes. */
    uint32_t tlv_crc32;    /**< CRC-32 of the TLV area. */
    uint8_t  uid[12];      /**< Chip UID words 0..2 in flash byte order. */
    uint32_t header_crc32; /**< CRC-32 of header bytes 0..27. */
} id_header_t;

/**
 * @brief Cached identity fields.
 * @note Only @c valid makes the other fields usable; a failed parse can leave
 *       partial data.
 */
typedef struct
{
    bool     valid;          /**< Set only after full validation. */
    uint16_t revision;       /**< Header revision. */
    char     serial[33];     /**< TLV 0x01, up to 32 chars. */
    char     product[33];    /**< TLV 0x02, up to 32 chars. */
    char     hw_revision[9]; /**< TLV 0x03, up to 8 chars. */
    char     profile[65];    /**< TLV 0x06, up to 64 chars. */
    uint8_t  mac[6];         /**< TLV 0x10, verbatim. */
    bool     mac_valid;      /**< A 6-byte MAC TLV was copied. */
} app_identity_t;

/** @brief Cached identity record. */
static app_identity_t identity;

/** @brief Set once a refresh has run, so a failed load is not retried. */
static bool refreshed;

/**
 * @brief Return the CRC-32 of header bytes 0..27.
 * @param[in] raw Raw header, at least 28 bytes.
 * @return Header CRC-32.
 */
static uint32_t header_crc(const uint8_t *raw) { return app_crc32(raw, 28U); }

/**
 * @brief Copy a TLV value into a NUL-terminated buffer, truncating silently.
 * @param[out] out    Destination buffer.
 * @param[in]  cap    Size of @p out in bytes, at least 1.
 * @param[in]  data   TLV value bytes.
 * @param[in]  length TLV value length in bytes.
 */
static void copy_text(char *out, size_t cap, const uint8_t *data, uint8_t length)
{
    size_t n = length;
    if(n >= cap)
    {
        n = cap - 1U;
    }
    memcpy(out, data, n);
    out[n] = '\0';
}

/**
 * @brief Validate the PRV1 record at @p address and cache it on success.
 * @details Invalidates the D-cache over the header first, and over the full
 *          header plus @ref ID_MAX_TLV window before reading TLVs. Unknown
 *          TLV types are skipped; a TLV running past the length fails.
 *
 * @param[in] address Flash address of the record header.
 *
 * @return true if the record passed every check and is cached.
 */
static bool parse_from(uint32_t address)
{
    uint8_t         raw[ID_HEADER_SIZE];
    const uint32_t *words = (const uint32_t *)address;

    SCB_InvalidateDCache_by_Addr((uint32_t *)address, ID_HEADER_SIZE);
    memcpy(raw, (const void *)address, sizeof(raw));
    if(words[0] != ID_MAGIC)
    {
        return false;
    }

    id_header_t header;
    memcpy(&header, raw, sizeof(header));
    if(header.format != ID_FORMAT || header.length == 0U || header.length > ID_MAX_TLV || header.length < 2U)
    {
        return false;
    }
    if(header_crc(raw) != header.header_crc32)
    {
        return false;
    }

    /* UID binding: the record has the three UID words in flash byte order. */
    uint8_t  uid[12];
    uint32_t uid_words[3] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};
    memcpy(uid, uid_words, sizeof(uid));
    if(memcmp(uid, header.uid, sizeof(uid)) != 0)
    {
        return false;
    }

    /* Invalidate the fixed full window: a length-based invalidation could
     * leave stale tail bytes in cache. */
    SCB_InvalidateDCache_by_Addr((uint32_t *)address, ID_HEADER_SIZE + ID_MAX_TLV);
    const uint8_t *tlv = (const uint8_t *)(address + ID_HEADER_SIZE);
    if(app_crc32(tlv, header.length) != header.tlv_crc32)
    {
        return false;
    }

    memset(&identity, 0, sizeof(identity));
    identity.revision = header.revision;
    size_t offset     = 0U;
    while(offset + 2U <= header.length)
    {
        uint8_t type   = tlv[offset];
        uint8_t length = tlv[offset + 1U];
        offset += 2U;
        if(type == 0U)
        {
            break;
        }
        if(offset + length > header.length)
        {
            return false;
        }
        const uint8_t *value = &tlv[offset];
        switch(type)
        {
        case ID_T_SERIAL: copy_text(identity.serial, sizeof(identity.serial), value, length); break;
        case ID_T_PRODUCT: copy_text(identity.product, sizeof(identity.product), value, length); break;
        case ID_T_HW_REV: copy_text(identity.hw_revision, sizeof(identity.hw_revision), value, length); break;
        case ID_T_PROFILE: copy_text(identity.profile, sizeof(identity.profile), value, length); break;
        case ID_T_MAC:
            if(length == 6U)
            {
                memcpy(identity.mac, value, 6U);
                identity.mac_valid = true;
            }
            break;
        default: break;
        }
        offset += length;
    }

    if(identity.serial[0] == '\0' || identity.product[0] == '\0')
    {
        return false;
    }
    identity.valid = true;
    return true;
}

/**
 * @brief Reload and validate the PRV1 record from flash.
 * @return true if either copy validated.
 * @warning Not thread-safe.
 */
bool app_identity_refresh(void)
{
    refreshed = true;
    memset(&identity, 0, sizeof(identity));
    if(parse_from(ID_IDENTITY_A))
    {
        return true;
    }
    return parse_from(ID_IDENTITY_SWAPPED);
}

/**
 * @brief Load the record on first use and return its validity.
 * @return true if a valid record is cached.
 * @note The first call rewrites the cache; do not race it.
 */
static bool ensure_loaded(void)
{
    if(!refreshed)
    {
        (void)app_identity_refresh();
    }
    return identity.valid;
}

/**
 * @brief Report whether a valid record is cached, loading it on first use.
 * @return true if a record passed every check.
 */
bool app_identity_valid(void) { return ensure_loaded(); }

/**
 * @brief Return the serial number.
 * @return NUL-terminated serial, or "".
 */
const char *app_identity_serial(void) { return ensure_loaded() ? identity.serial : ""; }

/**
 * @brief Return the product name.
 * @return NUL-terminated product name, or "".
 */
const char *app_identity_product(void) { return ensure_loaded() ? identity.product : ""; }

/**
 * @brief Return the hardware revision.
 * @return NUL-terminated revision, or "".
 */
const char *app_identity_hw_revision(void) { return ensure_loaded() ? identity.hw_revision : ""; }

/**
 * @brief Return the provisioning profile name.
 * @return NUL-terminated profile, or "".
 */
const char *app_identity_profile(void) { return ensure_loaded() ? identity.profile : ""; }

/**
 * @brief Return the header's provisioning revision.
 * @return Revision, or 0 without a valid record.
 */
uint16_t app_identity_revision(void) { return ensure_loaded() ? identity.revision : 0U; }

/**
 * @brief Copy the cached EUI-48.
 * @param[out] mac 6-byte destination.
 * @return true if copied.
 */
bool app_identity_mac(uint8_t mac[6])
{
    if(!ensure_loaded() || !identity.mac_valid)
    {
        return false;
    }
    memcpy(mac, identity.mac, 6U);
    return true;
}

/**
 * @brief Append a string to a bounded buffer, truncating silently.
 * @param[out] out  Destination buffer.
 * @param[in]  cap  Size of @p out in bytes, at least 1.
 * @param[in]  pos  Write offset, at most @p cap - 1.
 * @param[in]  text String to append; NULL appends nothing.
 * @return New write offset; @p out is NUL-terminated there.
 */
static size_t append_text(char *out, size_t cap, size_t pos, const char *text)
{
    while(text != NULL && *text != '\0' && pos + 1U < cap)
    {
        out[pos++] = *text++;
    }
    out[pos] = '\0';
    return pos;
}

/**
 * @brief Append an unsigned decimal number to a bounded buffer.
 * @param[out] out   Destination buffer.
 * @param[in]  cap   Size of @p out in bytes, at least 1.
 * @param[in]  pos   Write offset, at most @p cap - 1.
 * @param[in]  value Value to print.
 * @return New write offset; @p out is NUL-terminated there.
 */
static size_t append_uint(char *out, size_t cap, size_t pos, unsigned value)
{
    char   digits[10];
    size_t count = 0U;
    do
    {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while(value != 0U && count < sizeof(digits));
    while(count > 0U && pos + 1U < cap)
    {
        out[pos++] = digits[--count];
    }
    out[pos] = '\0';
    return pos;
}

/**
 * @brief Append the cached MAC as AA:BB:CC:DD:EE:FF.
 * @details Stops early if the buffer runs out. The caller checks
 *          @c identity.mac_valid.
 * @param[out] out Destination buffer.
 * @param[in]  cap Size of @p out in bytes, at least 1.
 * @param[in]  pos Write offset, at most @p cap - 1.
 * @return New write offset; @p out is NUL-terminated there.
 */
static size_t append_mac(char *out, size_t cap, size_t pos)
{
    static const char hex[] = "0123456789ABCDEF"; /**< Uppercase hex digits. */
    for(size_t i = 0U; i < 6U && pos + 3U < cap; i++)
    {
        if(i != 0U)
        {
            out[pos++] = ':';
        }
        out[pos++] = hex[(identity.mac[i] >> 4U) & 0xFU];
        out[pos++] = hex[identity.mac[i] & 0xFU];
    }
    out[pos] = '\0';
    return pos;
}

/**
 * @brief Write the HelloReply identity summary (A-05).
 * @param[out] out      Destination buffer.
 * @param[in]  capacity Size of @p out in bytes, including the NUL.
 * @return true if a valid record was summarised; false otherwise.
 * @note The comms task passes 192 bytes; unusually long fields truncate.
 */
bool app_identity_detail(char *out, size_t capacity)
{
    if(out == NULL || capacity == 0U)
    {
        return false;
    }
    out[0] = '\0';
    if(!ensure_loaded())
    {
        (void)append_text(out, capacity, 0U, "no identity");
        return false;
    }
    size_t pos = 0U;
    pos        = append_text(out, capacity, pos, "PRV1 serial=");
    pos        = append_text(out, capacity, pos, identity.serial);
    pos        = append_text(out, capacity, pos, ";mac=");
    if(identity.mac_valid)
    {
        pos = append_mac(out, capacity, pos);
    }
    pos = append_text(out, capacity, pos, ";hw=");
    pos = append_text(out, capacity, pos, identity.hw_revision);
    pos = append_text(out, capacity, pos, ";product=");
    pos = append_text(out, capacity, pos, identity.product);
    pos = append_text(out, capacity, pos, ";profile=");
    pos = append_text(out, capacity, pos, identity.profile);
    pos = append_text(out, capacity, pos, ";revision=");
    (void)append_uint(out, capacity, pos, identity.revision);
    return true;
}
