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
    uint8_t packet[512];
    size_t length = 99;

    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 512;
    config.max_packet_bytes = 512;
    config.max_tx_bytes = 512;
    bridge = zg_bridge_create(&config);
    CHECK(bridge != NULL);

    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_AGAIN);
    CHECK(length == 0);

    CHECK(zg_bridge_request_full(bridge) == ZG_BRIDGE_OK);
    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, 287, &length) ==
        ZG_BRIDGE_OVERSIZE);
    CHECK(length == 0);

    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_OK);
    CHECK(length == 288);
    CHECK(packet[ZG_BRIDGE_ENVELOPE_OFFSET_KIND] == ZG_BRIDGE_KIND_FULL);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_HEADER_BYTES) == 24);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_RECORD_COUNT) == 7);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_PAYLOAD_BYTES) == 264);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID) == 1);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_BASE_FRAME_ID) == 0);

    /* MAP at 24, total 22. */
    CHECK(le16(packet + 24 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0101);
    CHECK(le32(packet + 24 + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES) == 10);
    CHECK(le32(packet + 42) == 1);

    /* SELF at 46, total 71. */
    CHECK(le16(packet + 46 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0102);
    CHECK(le32(packet + 46 + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == 1);
    CHECK(le32(packet + 46 + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES) == 59);
    CHECK(packet[64] == (uint8_t)'T');
    CHECK(lef32(packet + 71) == 1.0f);
    CHECK(lef32(packet + 81) == 2.0f);
    CHECK(packet[91] == (uint8_t)'m');
    CHECK(packet[116] == 0);

    /* ACTOR 1001 at 117, total 46. */
    CHECK(le16(packet + 117 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0103);
    CHECK(le32(packet + 117 + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == 1001);
    CHECK(le32(packet + 117 + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES) == 34);
    CHECK(packet[135] == 1);
    CHECK(packet[142] == (uint8_t)'A');
    CHECK(lef32(packet + 149) == 3.0f);
    CHECK(lef32(packet + 159) == 4.0f);

    /* ACTOR 1002 at 163, total 46. */
    CHECK(le16(packet + 163 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0103);
    CHECK(le32(packet + 163 + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == 1002);
    CHECK(packet[188] == (uint8_t)'B');
    CHECK(lef32(packet + 195) == 5.0f);
    CHECK(lef32(packet + 205) == 6.0f);

    /* ACTOR_REMOVE 1002 at 209, total 12. */
    CHECK(le16(packet + 209 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0104);
    CHECK(le32(packet + 209 + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == 1002);
    CHECK(le32(packet + 209 + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES) == 0);

    /* EVENT at 221, total 29. */
    CHECK(le16(packet + 221 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0105);
    CHECK(le32(packet + 221 + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == 0);
    CHECK(le32(packet + 221 + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES) == 17);
    CHECK(le16(packet + 233) == 0x0050);
    CHECK(le32(packet + 239) == 1);
    CHECK(le16(packet + 243) == 0x0051);
    CHECK(packet[249] == 1);

    /* HUD at 250, total 38. */
    CHECK(le16(packet + 250 + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0106);
    CHECK(le32(packet + 250 + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == 0);
    CHECK(le32(packet + 250 + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES) == 26);
    CHECK(le16(packet + 262) == 0x0060);
    CHECK(le32(packet + 268) == 123);
    CHECK(le16(packet + 272) == 0x0062);
    CHECK(le16(packet + 278) == 2);
    CHECK(le16(packet + 280) == 0x0007);
    CHECK(le16(packet + 286) == 3);

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
    CHECK(length == 288);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID) == 2);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.render_packets == 2);

    zg_bridge_destroy(bridge);
    printf("RENDER-PASS records=7 bytes=288 actors=2 remove=1 events=1 hud=1\n");
    return 0;
}
