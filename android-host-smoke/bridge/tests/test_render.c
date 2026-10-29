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

static float lef32(const uint8_t *bytes)
{
    uint32_t bits = le32(bytes);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

int main(void)
{
    zg_bridge_config config;
    zg_bridge *bridge;
    zg_bridge_stats stats;
    uint8_t packet[128];
    size_t length = 99;

    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 128;
    config.max_packet_bytes = 128;
    config.max_tx_bytes = 128;
    bridge = zg_bridge_create(&config);
    CHECK(bridge != NULL);

    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_AGAIN);
    CHECK(length == 0);

    CHECK(zg_bridge_request_full(bridge) == ZG_BRIDGE_OK);
    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, 116, &length) ==
        ZG_BRIDGE_OVERSIZE);
    CHECK(length == 0);

    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_OK);
    CHECK(length == 117);
    CHECK(packet[ZG_BRIDGE_ENVELOPE_OFFSET_KIND] == ZG_BRIDGE_KIND_FULL);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_HEADER_BYTES) == 24);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_RECORD_COUNT) == 2);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_PAYLOAD_BYTES) == 93);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID) == 1);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_BASE_FRAME_ID) == 0);

    CHECK(le16(packet + 24 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0101);
    CHECK(le32(packet + 24 + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES) == 10);
    CHECK(le16(packet + 36) == 0x0100);
    CHECK(le16(packet + 40) == 4);
    CHECK(le32(packet + 42) == 1);

    CHECK(le16(packet + 46 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0102);
    CHECK(le32(packet + 46 + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == 1);
    CHECK(le32(packet + 46 + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES) == 59);
    CHECK(le16(packet + 58) == 0x0002);
    CHECK(packet[64] == (uint8_t)'T');
    CHECK(le16(packet + 65) == 0x0010);
    CHECK(lef32(packet + 71) == 1.0f);
    CHECK(le16(packet + 75) == 0x0012);
    CHECK(lef32(packet + 81) == 2.0f);
    CHECK(le16(packet + 85) == 0x0003);
    CHECK(packet[91] == (uint8_t)'m');
    CHECK(le16(packet + 92) == 0x0013);
    CHECK(lef32(packet + 98) == 0.0f);
    CHECK(le16(packet + 102) == 0x0007);
    CHECK(le16(packet + 108) == 1);
    CHECK(le16(packet + 110) == 0x0022);
    CHECK(packet[116] == 0);

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
    CHECK(length == 117);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID) == 2);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.render_packets == 2);

    zg_bridge_destroy(bridge);
    printf("RENDER-PASS full=117 map=1 self=T\n");
    return 0;
}
