#include "zg_bridge.h"
#include "zg_bridge_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FRAME-PASS FAIL line %d: %s\n", \
                __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

static size_t gzip_json(const char *json, uint8_t *out, size_t capacity)
{
    z_stream stream;
    int result;
    size_t length;

    memset(&stream, 0, sizeof stream);
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16,
            8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return 0;
    }
    stream.next_in = (Bytef *)json;
    stream.avail_in = (uInt)strlen(json);
    stream.next_out = out;
    stream.avail_out = (uInt)capacity;
    result = deflate(&stream, Z_FINISH);
    length = capacity - stream.avail_out;
    deflateEnd(&stream);
    return result == Z_STREAM_END ? length : 0;
}

static void write_be32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
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
    zg_bridge_config config;
    zg_bridge *bridge;
    zg_bridge_stats stats;
    zg_bridge_internal_json_stats json;
    const char *json_text = "{\"a\":1,\"b\":\"x\"}";
    uint8_t compressed[256];
    uint8_t frame[512];
    uint8_t control[24];
    size_t compressed_length;
    size_t frame_length;
    size_t half;

    compressed_length = gzip_json(json_text, compressed, sizeof compressed);
    CHECK(compressed_length > 0);
    frame_length = 4 + compressed_length;
    write_be32(frame, (uint32_t)compressed_length);
    memcpy(frame + 4, compressed, compressed_length);

    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 4096;
    config.max_packet_bytes = 2U * 1024U * 1024U;
    config.max_tx_bytes = 64;
    bridge = zg_bridge_create(&config);
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

    /* T40's 24-byte control envelope remains accepted without JSON parsing. */
    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 64;
    config.max_packet_bytes = 64;
    config.max_tx_bytes = 64;
    bridge = zg_bridge_create(&config);
    CHECK(bridge != NULL);
    write_control(control);
    CHECK(zg_bridge_feed(bridge, control, 10, 0) == ZG_BRIDGE_OK);
    CHECK(zg_bridge_feed(bridge, control + 10, 14, 0) == ZG_BRIDGE_OK);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 1);
    CHECK(stats.rx_bytes == 24);
    CHECK(stats.last_error == 0);
    zg_bridge_destroy(bridge);

    /* Oversize length prefix. */
    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 16;
    config.max_packet_bytes = 64;
    config.max_tx_bytes = 64;
    bridge = zg_bridge_create(&config);
    CHECK(bridge != NULL);
    frame[0] = 0;
    frame[1] = 0;
    frame[2] = 0;
    frame[3] = 17;
    CHECK(zg_bridge_feed(bridge, frame, 4, 0) == ZG_BRIDGE_OVERSIZE);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.last_error == ZG_BRIDGE_OVERSIZE);
    zg_bridge_destroy(bridge);

    /* Malformed gzip must return PROTOCOL_ERROR, not crash. */
    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 64;
    config.max_packet_bytes = 64;
    config.max_tx_bytes = 64;
    bridge = zg_bridge_create(&config);
    CHECK(bridge != NULL);
    frame[0] = 0;
    frame[1] = 0;
    frame[2] = 0;
    frame[3] = 4;
    frame[4] = 0xde;
    frame[5] = 0xad;
    frame[6] = 0xbe;
    frame[7] = 0xef;
    CHECK(zg_bridge_feed(bridge, frame, 8, 0) == ZG_BRIDGE_PROTOCOL_ERROR);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.frames == 0);
    CHECK(stats.last_error == ZG_BRIDGE_PROTOCOL_ERROR);
    zg_bridge_destroy(bridge);

    printf("FRAME-PASS tokens=%u keys=%u\n",
        (unsigned)json.tokens, (unsigned)json.keys);
    return 0;
}
