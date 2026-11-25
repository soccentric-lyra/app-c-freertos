/*
 * test_hwlink.c - HWLINK framing, COBS and CRC tests
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Target:  host (Linux/macOS) and CI
 *
 * These tests exercise the wire contract only.  The FlatBuffers payload is
 * opaque to the framing layer, so the tests use plain byte payloads and do not
 * need the HAL, FreeRTOS or the generated flatcc headers.
 */

#include <string.h>

#include "protocol/hwlink.h"
#include "services/crc32.h"
#include "test.h"

static int encode_decode_roundtrip(const uint8_t *payload, size_t len)
{
    uint8_t raw[HWLINK_MAX_FRAME];
    uint8_t encoded[HWLINK_MAX_ENCODED];
    uint8_t decoded[HWLINK_MAX_FRAME];
    uint8_t out[HWLINK_MAX_FRAME];
    size_t raw_len     = 0U;
    size_t encoded_len = 0U;
    size_t decoded_len = 0U;
    size_t out_len     = 0U;
    uint8_t flags      = 0U;
    uint16_t seq       = 0U;

    CHECK(hwlink_build_raw(payload, len, HWLINK_FLAG_RESPONSE, 0x1234U, raw,
                           sizeof(raw), &raw_len));
    CHECK_U32(raw_len, 8U + len);
    CHECK_U32(raw[0], HWLINK_PROTOCOL_MAJOR);
    CHECK_U32(raw[1], HWLINK_FLAG_RESPONSE);
    CHECK_U32(raw[2], 0x34U);
    CHECK_U32(raw[3], 0x12U);

    CHECK(hwlink_cobs_encode(raw, raw_len, encoded, sizeof(encoded),
                             &encoded_len));
    CHECK(encoded_len >= raw_len + 2U);
    CHECK_U32(encoded[encoded_len - 1U], 0U);
    for(size_t i = 0; i < encoded_len - 1U; i++)
    {
        CHECK(encoded[i] != 0U);
    }

    CHECK(hwlink_cobs_decode(encoded, encoded_len - 1U, decoded,
                             sizeof(decoded), &decoded_len) == 0);
    CHECK_U32(decoded_len, raw_len);
    CHECK(memcmp(decoded, raw, raw_len) == 0);

    CHECK(hwlink_decode_raw(decoded, decoded_len, out, sizeof(out), &out_len,
                            &flags, &seq));
    CHECK_U32(out_len, len);
    CHECK_U32(flags, HWLINK_FLAG_RESPONSE);
    CHECK_U32(seq, 0x1234U);
    CHECK(memcmp(out, payload, len) == 0);
    return 1;
}

int test_hwlink(void)
{
    static const uint8_t payload[] = {0x00, 0x01, 0x02, 0xFE, 0xFF, 0x00};
    uint8_t              raw[HWLINK_MAX_FRAME];
    uint8_t              encoded[HWLINK_MAX_ENCODED];
    uint8_t              decoded[HWLINK_MAX_FRAME];
    uint8_t              out[8];
    uint8_t              flags   = 0U;
    uint16_t             seq     = 0U;
    size_t               raw_len = 0U;
    size_t               enc_len = 0U;
    size_t               dec_len = 0U;
    size_t               out_len = 0U;

    /* Known CRC vector: the framing CRC is the same CRC-32/ISO-HDLC. */
    CHECK_U32(app_crc32((const uint8_t *)"123456789", 9U), 0xCBF43926UL);

    /* Empty payload is a legal eight-byte frame. */
    CHECK(encode_decode_roundtrip((const uint8_t *)"", 0U));
    CHECK(encode_decode_roundtrip(payload, sizeof(payload)));

    /* COBS examples from the specification: a lone zero and a short run. */
    {
        static const uint8_t zero[]   = {0x00};
        static const uint8_t ones[]   = {0x01, 0x02, 0x03};
        static const uint8_t spread[] = {0x01, 0x00, 0x02};
        uint8_t              buf[16];
        size_t               n = 0U;

        CHECK(hwlink_cobs_encode(zero, sizeof(zero), buf, sizeof(buf), &n));
        CHECK_U32(n, 3U);
        CHECK_U32(buf[0], 0x01U);
        CHECK_U32(buf[1], 0x01U);
        CHECK_U32(buf[2], 0x00U);

        CHECK(hwlink_cobs_encode(ones, sizeof(ones), buf, sizeof(buf), &n));
        CHECK_U32(n, 5U);
        CHECK_U32(buf[0], 0x04U);
        CHECK_U32(buf[4], 0x00U);

        CHECK(hwlink_cobs_encode(spread, sizeof(spread), buf, sizeof(buf), &n));
        CHECK_U32(n, 5U);
        CHECK_U32(buf[0], 0x02U);
        CHECK_U32(buf[1], 0x01U);
        CHECK_U32(buf[2], 0x02U);
        CHECK_U32(buf[3], 0x02U);
        CHECK_U32(buf[4], 0x00U);

        /* Malformed bodies are rejected. */
        CHECK(hwlink_cobs_decode((const uint8_t *)"\x00", 1U, decoded,
                                 sizeof(decoded), &dec_len) == -1);
        CHECK(hwlink_cobs_decode((const uint8_t *)"\x03\x01", 2U, decoded,
                                 sizeof(decoded), &dec_len) == -1);
        CHECK(hwlink_cobs_decode((const uint8_t *)"\x05\x01", 2U, decoded,
                                 sizeof(decoded), &dec_len) == -1);
    }

    /* COBS boundary cases: 253, 254 and 255 non-zero bytes, plus a zero at
     * the end so the trailing empty block is exercised. */
    {
        uint8_t big[HWLINK_MAX_FRAME];
        uint8_t enc[HWLINK_MAX_ENCODED];
        uint8_t dec[HWLINK_MAX_FRAME];
        size_t  n = 0U;
        size_t  m = 0U;

        for(size_t len = 253U; len <= 255U; len++)
        {
            memset(big, 0x5AU, len);
            CHECK(hwlink_cobs_encode(big, len, enc, sizeof(enc), &n));
            CHECK_U32(enc[n - 1U], 0U);
            CHECK(hwlink_cobs_decode(enc, n - 1U, dec, sizeof(dec), &m) == 0);
            CHECK_U32(m, len);
            CHECK(memcmp(big, dec, len) == 0);
        }

        memset(big, 0x5AU, 16U);
        big[15] = 0U;
        CHECK(hwlink_cobs_encode(big, 16U, enc, sizeof(enc), &n));
        CHECK(hwlink_cobs_decode(enc, n - 1U, dec, sizeof(dec), &m) == 0);
        CHECK_U32(m, 16U);
        CHECK(memcmp(big, dec, 16U) == 0);
    }

    /* Argument and capacity failures must not write a frame. */
    CHECK(!hwlink_build_raw(NULL, 1U, 0U, 0U, raw, sizeof(raw), &raw_len));
    CHECK(!hwlink_build_raw(payload, sizeof(payload), 0U, 0U, NULL,
                            sizeof(raw), &raw_len));
    CHECK(!hwlink_build_raw(payload, sizeof(payload), 0U, 0U, raw,
                            sizeof(payload) + 7U, &raw_len));
    CHECK(!hwlink_build_raw(payload, HWLINK_MAX_FRAME - 7U, 0U, 0U, raw,
                            sizeof(raw), &raw_len));
    CHECK(!hwlink_cobs_encode(NULL, 1U, encoded, sizeof(encoded), &enc_len));
    CHECK(!hwlink_cobs_encode(raw, 0U, encoded, sizeof(encoded), &enc_len));
    CHECK(!hwlink_cobs_encode(raw, 1U, encoded, 1U, &enc_len));
    CHECK(!hwlink_cobs_encode(raw, 1U, encoded, 2U, &enc_len));

    /* The largest legal payload fits; one byte more does not. */
    {
        uint8_t max_payload[HWLINK_MAX_FRAME - 8U];
        memset(max_payload, 0x11, sizeof(max_payload));
        CHECK(hwlink_build_raw(max_payload, sizeof(max_payload), 0U, 1U, raw,
                               sizeof(raw), &raw_len));
        CHECK_U32(raw_len, HWLINK_MAX_FRAME);
    }

    /* Corrupted CRC, wrong protocol version and reserved flags are rejected
     * before the payload is copied. */
    CHECK(hwlink_build_raw(payload, sizeof(payload), HWLINK_FLAG_EVENT, 7U,
                           raw, sizeof(raw), &raw_len));
    raw[raw_len - 1U] ^= 0xFFU;
    out_len = 0xDEADBEEFU;
    CHECK(!hwlink_decode_raw(raw, raw_len, out, sizeof(out), &out_len, &flags,
                             &seq));
    CHECK_U32(out_len, 0xDEADBEEFU); /* not modified on failure */

    CHECK(hwlink_build_raw(payload, sizeof(payload), 0U, 7U, raw, sizeof(raw),
                           &raw_len));
    raw[0] = HWLINK_PROTOCOL_MAJOR + 1U;
    /* Repair the CRC so only the protocol version is wrong. */
    {
        uint32_t crc = app_crc32(raw, raw_len - 4U);
        raw[raw_len - 4U] = (uint8_t)(crc & 0xFFU);
        raw[raw_len - 3U] = (uint8_t)((crc >> 8U) & 0xFFU);
        raw[raw_len - 2U] = (uint8_t)((crc >> 16U) & 0xFFU);
        raw[raw_len - 1U] = (uint8_t)((crc >> 24U) & 0xFFU);
    }
    CHECK(!hwlink_decode_raw(raw, raw_len, out, sizeof(out), &out_len, &flags,
                             &seq));

    CHECK(hwlink_build_raw(payload, sizeof(payload), 0x04U, 7U, raw,
                           sizeof(raw), &raw_len));
    CHECK(!hwlink_decode_raw(raw, raw_len, out, sizeof(out), &out_len, &flags,
                             &seq));

    /* A too-small payload destination is rejected. */
    CHECK(hwlink_build_raw(payload, sizeof(payload), 0U, 7U, raw, sizeof(raw),
                           &raw_len));
    CHECK(!hwlink_decode_raw(raw, raw_len, out, 2U, &out_len, &flags, &seq));

    return 1;
}
