#include "zg_bridge.h"
#include "zg_bridge_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FRAME-FAIL line %d: %s\n", \
                __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

static size_t gzip_bytes(const uint8_t *data, size_t length, uint8_t *out,
    size_t capacity)
{
    z_stream stream;
    int result;
    size_t produced;

    memset(&stream, 0, sizeof stream);
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16,
            8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return 0;
    }
    stream.next_in = (Bytef *)data;
    stream.avail_in = (uInt)length;
    stream.next_out = out;
    stream.avail_out = (uInt)capacity;
    result = deflate(&stream, Z_FINISH);
    produced = capacity - stream.avail_out;
    deflateEnd(&stream);
    return result == Z_STREAM_END ? produced : 0;
}

static size_t build_frame(const uint8_t *body, size_t body_length, uint8_t *out,
    size_t capacity)
{
    if (capacity < body_length + 4) {
        return 0;
    }
    out[0] = (uint8_t)(body_length >> 24);
    out[1] = (uint8_t)(body_length >> 16);
    out[2] = (uint8_t)(body_length >> 8);
    out[3] = (uint8_t)body_length;
    memcpy(out + 4, body, body_length);
    return body_length + 4;
}

static zg_bridge *make_bridge(uint32_t max_frame, uint32_t max_packet)
{
    zg_bridge_config config;
    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = max_frame;
    config.max_packet_bytes = max_packet;
    config.max_tx_bytes = 64;
    return zg_bridge_create(&config);
}

static void write_control(uint8_t *out)
{
    memset(out, 0, 24);
    out[0] = 0x47;
    out[1] = 0x5a;
    out[2] = 1;
    out[3] = 3;
    out[6] = 24;
}

int main(void)
{
    const char *json_text = "{\"a\":1,\"b\":\"x\"}";
    uint8_t compressed[256];
    uint8_t frame[512];
    uint8_t multi[1024];
    uint8_t control[24];
    uint8_t *big_json = NULL;
    uint8_t *big_gzip = NULL;
    uint8_t *big_frame = NULL;
    zg_bridge *bridge;
    zg_bridge_stats stats;
    zg_bridge_internal_json_stats json;
    size_t compressed_length;
    size_t frame_length;
    size_t half;
    size_t i;

    compressed_length = gzip_bytes((const uint8_t *)json_text,
        strlen(json_text), compressed, sizeof compressed);
    CHECK(compressed_length > 0);
    frame_length = build_frame(compressed, compressed_length, frame,
        sizeof frame);
    CHECK(frame_length > 0);

    /* Normal frame, split header/body. */
    bridge = make_bridge(4096, 2U * 1024U * 1024U);
    CHECK(bridge != NULL);
    CHECK(zg_bridge_feed(bridge, frame, 2, 0) == ZG_BRIDGE_OK);
    CHECK(zg_bridge_feed(bridge, frame + 2, 2, 0) == ZG_BRIDGE_OK);
    half = compressed_length / 2;
    CHECK(zg_bridge_feed(bridge, frame + 4, half, 0) == ZG_BRIDGE_OK);
    CHECK(zg_bridge_feed(bridge, frame + 4 + half,
        compressed_length - half, 0) == ZG_BRIDGE_OK);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 1);
    CHECK(stats.rx_bytes == frame_length);
    CHECK(stats.last_error == 0);
    zg_bridge_internal_get_json_stats(bridge, &json);
    CHECK(json.tokens > 0);
    CHECK(json.keys == 2);
    zg_bridge_destroy(bridge);

    /* One byte at a time. */
    bridge = make_bridge(4096, 2U * 1024U * 1024U);
    CHECK(bridge != NULL);
    for (i = 0; i < frame_length; ++i) {
        CHECK(zg_bridge_feed(bridge, frame + i, 1, 0) == ZG_BRIDGE_OK);
    }
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 1);
    CHECK(stats.last_error == 0);
    zg_bridge_destroy(bridge);

    /* Two complete frames in one feed. */
    CHECK(frame_length * 2 <= sizeof multi);
    memcpy(multi, frame, frame_length);
    memcpy(multi + frame_length, frame, frame_length);
    bridge = make_bridge(4096, 2U * 1024U * 1024U);
    CHECK(bridge != NULL);
    CHECK(zg_bridge_feed(bridge, multi, frame_length * 2, 0) == ZG_BRIDGE_OK);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 2);
    CHECK(stats.last_error == 0);
    zg_bridge_destroy(bridge);

    /* Trailing bytes after gzip member must fail. */
    CHECK(compressed_length + 7 <= sizeof compressed);
    memcpy(compressed + compressed_length, "GARBAGE", 7);
    frame_length = build_frame(compressed, compressed_length + 7, frame,
        sizeof frame);
    CHECK(frame_length > 0);
    bridge = make_bridge(4096, 2U * 1024U * 1024U);
    CHECK(bridge != NULL);
    CHECK(zg_bridge_feed(bridge, frame, frame_length, 0) ==
        ZG_BRIDGE_PROTOCOL_ERROR);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 0);
    CHECK(stats.last_error == ZG_BRIDGE_PROTOCOL_ERROR);
    zg_bridge_destroy(bridge);

    /* Invalid UTF-8 in a JSON string must fail: 0x80, overlong C0 AF, 0xFF. */
    {
        const uint8_t bad1[] = {'{', '"', 'a', '"', ':', '"', 0x80, '"', '}'};
        const uint8_t bad2[] = {'{', '"', 'a', '"', ':', '"', 0xc0, 0xaf, '"', '}'};
        const uint8_t bad3[] = {'{', '"', 'a', '"', ':', '"', 0xff, '"', '}'};
        const uint8_t *bad[] = {bad1, bad2, bad3};
        const size_t bad_lengths[] = {sizeof bad1, sizeof bad2, sizeof bad3};
        for (i = 0; i < 3; ++i) {
            compressed_length = gzip_bytes(bad[i], bad_lengths[i], compressed,
                sizeof compressed);
            CHECK(compressed_length > 0);
            frame_length = build_frame(compressed, compressed_length, frame,
                sizeof frame);
            CHECK(frame_length > 0);
            bridge = make_bridge(4096, 2U * 1024U * 1024U);
            CHECK(bridge != NULL);
            CHECK(zg_bridge_feed(bridge, frame, frame_length, 0) ==
                ZG_BRIDGE_PROTOCOL_ERROR);
            zg_bridge_get_stats(bridge, &stats);
            CHECK(stats.frames == 0);
            CHECK(stats.last_error == ZG_BRIDGE_PROTOCOL_ERROR);
            zg_bridge_destroy(bridge);
        }
    }

    /* Token cap: a large number array must be rejected, not counted forever. */
    {
        const size_t count = 300000;
        size_t length = 2 + (count * 2);
        size_t position = 0;
        size_t big_gzip_length;
        size_t big_frame_length;

        big_json = (uint8_t *)malloc(length + 1);
        big_gzip = (uint8_t *)malloc(1024 * 1024);
        big_frame = (uint8_t *)malloc(1024 * 1024);
        CHECK(big_json != NULL && big_gzip != NULL && big_frame != NULL);
        big_json[position++] = '[';
        for (i = 0; i < count; ++i) {
            big_json[position++] = '0';
            if (i + 1 < count) {
                big_json[position++] = ',';
            }
        }
        big_json[position++] = ']';
        big_json[position] = 0;
        big_gzip_length = gzip_bytes(big_json, position, big_gzip,
            1024 * 1024);
        CHECK(big_gzip_length > 0);
        big_frame_length = build_frame(big_gzip, big_gzip_length, big_frame,
            1024 * 1024);
        CHECK(big_frame_length > 0);
        bridge = make_bridge(4096, 2U * 1024U * 1024U);
        CHECK(bridge != NULL);
        CHECK(zg_bridge_feed(bridge, big_frame, big_frame_length, 0) ==
            ZG_BRIDGE_PROTOCOL_ERROR);
        zg_bridge_get_stats(bridge, &stats);
        CHECK(stats.frames == 0);
        CHECK(stats.last_error == ZG_BRIDGE_PROTOCOL_ERROR);
        zg_bridge_destroy(bridge);

        /* The same large plaintext exceeds a small output cap. */
        bridge = make_bridge(4096, 64);
        CHECK(bridge != NULL);
        CHECK(zg_bridge_feed(bridge, big_frame, big_frame_length, 0) ==
            ZG_BRIDGE_OVERSIZE);
        zg_bridge_destroy(bridge);
        free(big_json);
        free(big_gzip);
        free(big_frame);
        big_json = NULL;
        big_gzip = NULL;
        big_frame = NULL;
    }

    /* Feed-level oversize/error clears RX; a later valid frame recovers. */
    compressed_length = gzip_bytes((const uint8_t *)json_text,
        strlen(json_text), compressed, sizeof compressed);
    frame_length = build_frame(compressed, compressed_length, frame,
        sizeof frame);
    CHECK(frame_length > 0);
    bridge = make_bridge(64, 2U * 1024U * 1024U);
    CHECK(bridge != NULL);
    memset(multi, 0, sizeof multi);
    CHECK(zg_bridge_feed(bridge, multi, 69, 0) == ZG_BRIDGE_OVERSIZE);
    CHECK(zg_bridge_feed(bridge, frame, frame_length, 0) == ZG_BRIDGE_OK);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 1);
    zg_bridge_destroy(bridge);

    /* Malformed gzip, then a valid frame on the same bridge. */
    bridge = make_bridge(4096, 2U * 1024U * 1024U);
    CHECK(bridge != NULL);
    memset(frame, 0, 8);
    frame[3] = 4;
    frame[4] = 0xde;
    frame[5] = 0xad;
    frame[6] = 0xbe;
    frame[7] = 0xef;
    CHECK(zg_bridge_feed(bridge, frame, 8, 0) == ZG_BRIDGE_PROTOCOL_ERROR);
    CHECK(zg_bridge_feed(bridge, multi, 0, 0) == ZG_BRIDGE_OK);
    compressed_length = gzip_bytes((const uint8_t *)json_text,
        strlen(json_text), compressed, sizeof compressed);
    frame_length = build_frame(compressed, compressed_length, frame,
        sizeof frame);
    CHECK(zg_bridge_feed(bridge, frame, frame_length, 0) == ZG_BRIDGE_OK);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 1);
    zg_bridge_destroy(bridge);

    /* Truncated gzip must fail. */
    compressed_length = gzip_bytes((const uint8_t *)json_text,
        strlen(json_text), compressed, sizeof compressed);
    CHECK(compressed_length > 4);
    frame_length = build_frame(compressed, compressed_length - 4, frame,
        sizeof frame);
    CHECK(frame_length > 0);
    bridge = make_bridge(4096, 2U * 1024U * 1024U);
    CHECK(bridge != NULL);
    CHECK(zg_bridge_feed(bridge, frame, frame_length, 0) ==
        ZG_BRIDGE_PROTOCOL_ERROR);
    zg_bridge_destroy(bridge);

    /* T40 control envelope, split across feeds. */
    bridge = make_bridge(64, 64);
    CHECK(bridge != NULL);
    write_control(control);
    CHECK(zg_bridge_feed(bridge, control, 10, 0) == ZG_BRIDGE_OK);
    CHECK(zg_bridge_feed(bridge, control + 10, 14, 0) == ZG_BRIDGE_OK);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 1);
    CHECK(stats.rx_bytes == 24);
    CHECK(stats.last_error == 0);
    zg_bridge_destroy(bridge);

    /* Length zero and 0xffffffff are rejected without crashing. */
    bridge = make_bridge(4096, 64);
    CHECK(bridge != NULL);
    memset(frame, 0, 4);
    CHECK(zg_bridge_feed(bridge, frame, 4, 0) == ZG_BRIDGE_PROTOCOL_ERROR);
    frame[0] = 0xff;
    frame[1] = 0xff;
    frame[2] = 0xff;
    frame[3] = 0xff;
    CHECK(zg_bridge_feed(bridge, frame, 4, 0) == ZG_BRIDGE_OVERSIZE);
    zg_bridge_destroy(bridge);

    printf("FRAME-PASS tokens=%u keys=%u\n",
        (unsigned)json.tokens, (unsigned)json.keys);
    return 0;
}
