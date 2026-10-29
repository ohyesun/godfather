#include "zg_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "RENDER-FAIL line %d: %s\n", \
                __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

static uint16_t le16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

int main(void)
{
    zg_bridge_config config;
    zg_bridge *bridge;
    zg_bridge_stats stats;
    uint8_t packet[24];
    size_t length = 99;

    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 64;
    config.max_packet_bytes = 64;
    config.max_tx_bytes = 64;
    bridge = zg_bridge_create(&config);
    CHECK(bridge != NULL);

    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_AGAIN);
    CHECK(length == 0);

    CHECK(zg_bridge_request_full(bridge) == ZG_BRIDGE_OK);
    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, 23, &length) ==
        ZG_BRIDGE_OVERSIZE);
    CHECK(length == 0);

    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_OK);
    CHECK(length == 24);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_MAGIC) ==
        ZG_BRIDGE_MAGIC);
    CHECK(packet[ZG_BRIDGE_ENVELOPE_OFFSET_VERSION] ==
        ZG_BRIDGE_ENVELOPE_VERSION);
    CHECK(packet[ZG_BRIDGE_ENVELOPE_OFFSET_KIND] ==
        ZG_BRIDGE_KIND_CONTROL);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FLAGS) == 0);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_HEADER_BYTES) == 24);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID) == 1);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_BASE_FRAME_ID) == 0);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_PAYLOAD_BYTES) == 0);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_RECORD_COUNT) == 0);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_RESERVED) == 0);

    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_AGAIN);
    CHECK(length == 0);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.render_packets == 1);

    CHECK(zg_bridge_request_full(bridge) == ZG_BRIDGE_OK);
    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_OK);
    CHECK(length == 24);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID) == 2);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.render_packets == 2);

    zg_bridge_destroy(bridge);
    printf("RENDER-PASS\n");
    return 0;
}
