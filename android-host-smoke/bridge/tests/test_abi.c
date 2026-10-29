#include "zg_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "BRIDGE-ABI FAIL line %d: %s\n", \
                __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

_Static_assert(ZG_BRIDGE_ABI_VERSION == 1U, "ABI version");
_Static_assert(ZG_BRIDGE_MAGIC == 0x5a47U, "magic");
_Static_assert(ZG_BRIDGE_ENVELOPE_VERSION == 1U, "envelope version");
_Static_assert(ZG_BRIDGE_ENVELOPE_HEADER_BYTES == 24U, "header bytes");
_Static_assert(ZG_BRIDGE_ENVELOPE_SIZE == 24U, "envelope size");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_MAGIC == 0U, "magic offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_VERSION == 2U, "version offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_KIND == 3U, "kind offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_FLAGS == 4U, "flags offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_HEADER_BYTES == 6U, "header offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID == 8U, "frame offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_BASE_FRAME_ID == 12U, "base offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_PAYLOAD_BYTES == 16U, "payload offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_RECORD_COUNT == 20U, "record count offset");
_Static_assert(ZG_BRIDGE_ENVELOPE_OFFSET_RESERVED == 22U, "reserved offset");
_Static_assert(ZG_BRIDGE_RECORD_HEADER_BYTES == 12U, "record header");
_Static_assert(ZG_BRIDGE_RECORD_OFFSET_TYPE == 0U, "record type offset");
_Static_assert(ZG_BRIDGE_RECORD_OFFSET_SCHEMA == 2U, "record schema offset");
_Static_assert(ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID == 4U, "record id offset");
_Static_assert(ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES == 8U, "record length offset");
_Static_assert(ZG_BRIDGE_KIND_FULL == 1U, "full kind");
_Static_assert(ZG_BRIDGE_KIND_DELTA == 2U, "delta kind");
_Static_assert(ZG_BRIDGE_KIND_CONTROL == 3U, "control kind");
_Static_assert(ZG_BRIDGE_KIND_ERROR == 4U, "error kind");
_Static_assert(ZG_BRIDGE_FLAG_KEYFRAME == 1U, "keyframe flag");
_Static_assert(ZG_BRIDGE_FLAG_RESYNC == 2U, "resync flag");
_Static_assert(ZG_BRIDGE_OK == 0, "status ok");
_Static_assert(ZG_BRIDGE_NEED_MORE == 1, "status need more");
_Static_assert(ZG_BRIDGE_AGAIN == 2, "status again");
_Static_assert(ZG_BRIDGE_RESYNC_REQUIRED == 3, "status resync");
_Static_assert(ZG_BRIDGE_PROTOCOL_ERROR == 4, "status protocol");
_Static_assert(ZG_BRIDGE_OVERSIZE == 5, "status oversize");
_Static_assert(ZG_BRIDGE_BACKPRESSURE == 6, "status backpressure");
_Static_assert(ZG_BRIDGE_AUTH_REQUIRED == 7, "status auth");
_Static_assert(ZG_BRIDGE_CLOSED == 8, "status closed");

int main(void)
{
    zg_bridge_config config;
    zg_bridge *bridge;
    zg_bridge_stats stats;
    uint8_t input[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t output[8];
    size_t out_length = 99;
    size_t i;

    memset(&config, 0, sizeof config);
    CHECK(zg_bridge_create(NULL) == NULL);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 8;
    config.max_packet_bytes = 16;
    config.max_tx_bytes = 8;
    config.flags = 0;
    bridge = zg_bridge_create(&config);
    CHECK(bridge != NULL);

    memset(&stats, 0, sizeof stats);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.rx_bytes == 0 && stats.tx_bytes == 0 && stats.frames == 0);

    CHECK(zg_bridge_feed(bridge, input, 9, 0) == ZG_BRIDGE_OVERSIZE);
    CHECK(zg_bridge_feed(bridge, NULL, 1, 0) == ZG_BRIDGE_PROTOCOL_ERROR);
    CHECK(zg_bridge_feed(bridge, input, 8, 0) == ZG_BRIDGE_OK);
    CHECK(zg_bridge_feed(bridge, input, 1, 0) == ZG_BRIDGE_BACKPRESSURE);

    out_length = 99;
    CHECK(zg_bridge_poll_render(bridge, output, sizeof output, &out_length) ==
        ZG_BRIDGE_AGAIN);
    CHECK(out_length == 0);
    out_length = 99;
    CHECK(zg_bridge_poll_render(bridge, output, sizeof output, NULL) ==
        ZG_BRIDGE_PROTOCOL_ERROR);
    out_length = 99;
    CHECK(zg_bridge_poll_tx(bridge, output, sizeof output, &out_length) ==
        ZG_BRIDGE_AGAIN);
    CHECK(out_length == 0);

    CHECK(zg_bridge_queue_input(bridge, input, 9, 0) == ZG_BRIDGE_OVERSIZE);
    CHECK(zg_bridge_queue_input(bridge, NULL, 1, 0) == ZG_BRIDGE_PROTOCOL_ERROR);
    CHECK(zg_bridge_queue_input(bridge, input, 8, 0) == ZG_BRIDGE_OK);
    CHECK(zg_bridge_queue_input(bridge, input, 1, 0) == ZG_BRIDGE_BACKPRESSURE);

    out_length = 99;
    CHECK(zg_bridge_poll_tx(bridge, output, 4, &out_length) ==
        ZG_BRIDGE_BACKPRESSURE);
    CHECK(out_length == 0);
    out_length = 99;
    CHECK(zg_bridge_poll_tx(bridge, output, sizeof output, &out_length) ==
        ZG_BRIDGE_OK);
    CHECK(out_length == 8);
    for (i = 0; i < sizeof output; ++i) {
        CHECK(output[i] == input[i]);
    }
    out_length = 99;
    CHECK(zg_bridge_poll_tx(bridge, output, sizeof output, &out_length) ==
        ZG_BRIDGE_AGAIN);
    CHECK(out_length == 0);

    CHECK(zg_bridge_request_full(bridge) == ZG_BRIDGE_OK);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.rx_bytes == 8);
    CHECK(stats.tx_bytes == 8);
    CHECK(stats.frames == 1);
    CHECK(stats.render_packets == 0);
    CHECK(stats.render_dropped == 0);
    CHECK(stats.render_queue_depth == 0);
    CHECK(stats.tx_queue_depth == 0);
    CHECK(stats.last_error == ZG_BRIDGE_BACKPRESSURE);

    zg_bridge_destroy(bridge);

    config.flags = 1;
    CHECK(zg_bridge_create(&config) == NULL);
    config.flags = 0;
    config.abi_version = 2;
    CHECK(zg_bridge_create(&config) == NULL);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 0;
    CHECK(zg_bridge_create(&config) == NULL);
    config.max_frame_bytes = 8;
    config.max_packet_bytes = 0;
    CHECK(zg_bridge_create(&config) == NULL);
    config.max_packet_bytes = 16;
    config.max_tx_bytes = 0;
    CHECK(zg_bridge_create(&config) == NULL);
    config.max_tx_bytes = 8;
    config.max_frame_bytes = ZG_BRIDGE_MAX_LIMIT_BYTES + 1;
    CHECK(zg_bridge_create(&config) == NULL);

    printf("BRIDGE-ABI PASS\n");
    return 0;
}
