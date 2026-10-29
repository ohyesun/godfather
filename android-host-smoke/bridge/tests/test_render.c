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

#define PACKET_BYTES 829
#define MAP_OFFSET 24
#define SELF_OFFSET 46
#define ACTOR_OFFSET 158

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

static const uint8_t *field(const uint8_t *record, uint32_t payload_bytes,
    uint16_t field_id, uint8_t *type, uint16_t *value_length)
{
    const uint8_t *cursor = record + ZG_BRIDGE_RECORD_HEADER_BYTES;
    const uint8_t *end = cursor + payload_bytes;

    while (cursor + 6 <= end) {
        uint16_t id = le16(cursor);
        uint8_t field_type = cursor[2];
        uint16_t length = le16(cursor + 4);
        const uint8_t *value = cursor + 6;
        if (value + length > end) {
            return NULL;
        }
        if (id == field_id) {
            if (type != NULL) {
                *type = field_type;
            }
            if (value_length != NULL) {
                *value_length = length;
            }
            return value;
        }
        cursor = value + length;
    }
    return NULL;
}

static int check_string_field(const uint8_t *record, uint32_t payload_bytes,
    uint16_t field_id, const char *expected)
{
    uint8_t type = 0;
    uint16_t length = 0;
    const uint8_t *value = field(record, payload_bytes, field_id, &type,
        &length);
    return value != NULL && type == 6 && length == strlen(expected) &&
        memcmp(value, expected, length) == 0;
}

int main(void)
{
    zg_bridge_config config;
    zg_bridge *bridge;
    zg_bridge_stats stats;
    uint8_t packet[1024];
    size_t length = 99;
    uint16_t value_length;
    uint8_t type;
    const uint8_t *value;
    static const struct {
        uint32_t id;
        const char *name;
        float x;
        float z;
        uint16_t level;
    } actors[] = {
        {792, "听泉寨打手", 198.0f, 398.0f, 14},
        {791, "听泉寨打手", 199.0f, 403.0f, 14},
        {803, "听泉寨头目", 198.0f, 416.0f, 14},
        {800, "听泉寨头目", 203.0f, 412.0f, 14},
        {799, "听泉寨头目", 200.0f, 401.0f, 14},
        {773, "听泉寨山贼", 206.0f, 400.0f, 12},
        {801, "听泉寨头目", 202.0f, 401.0f, 14},
        {772, "听泉寨山贼", 193.0f, 406.0f, 12},
        {769, "听泉寨山贼", 193.0f, 414.0f, 12},
        {99579, "兔子", 186.0f, 395.0f, 1},
    };
    const uint8_t *record;
    uint32_t payload;
    size_t i;

    memset(&config, 0, sizeof config);
    config.abi_version = ZG_BRIDGE_ABI_VERSION;
    config.max_frame_bytes = 1024;
    config.max_packet_bytes = 1024;
    config.max_tx_bytes = 1024;
    bridge = zg_bridge_create(&config);
    CHECK(bridge != NULL);

    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_AGAIN);
    CHECK(length == 0);
    CHECK(zg_bridge_request_full(bridge) == ZG_BRIDGE_OK);
    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, 828, &length) ==
        ZG_BRIDGE_OVERSIZE);
    CHECK(length == 0);
    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_OK);
    CHECK(length == PACKET_BYTES);
    CHECK(packet[ZG_BRIDGE_ENVELOPE_OFFSET_KIND] == ZG_BRIDGE_KIND_FULL);
    CHECK(le16(packet + ZG_BRIDGE_ENVELOPE_OFFSET_RECORD_COUNT) == 12);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_PAYLOAD_BYTES) == 805);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID) == 1);

    record = packet + MAP_OFFSET;
    payload = le32(record + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES);
    CHECK(le16(record + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0101);
    CHECK(payload == 10);
    value = field(record, payload, 0x0100, &type, &value_length);
    CHECK(value != NULL && type == 3 && value_length == 4 && le32(value) == 2);

    record = packet + SELF_OFFSET;
    payload = le32(record + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES);
    CHECK(le16(record + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0102);
    CHECK(le32(record + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == 1);
    CHECK(payload == 100);
    CHECK(check_string_field(record, payload, 0x0002, "烈火教父"));
    value = field(record, payload, 0x0010, &type, &value_length);
    CHECK(value != NULL && type == 5 && value_length == 4 &&
        lef32(value) == 186.0f);
    value = field(record, payload, 0x0011, &type, &value_length);
    CHECK(value != NULL && type == 5 && value_length == 4 &&
        lef32(value) == 0.0f);
    value = field(record, payload, 0x0012, &type, &value_length);
    CHECK(value != NULL && type == 5 && value_length == 4 &&
        lef32(value) == 395.0f);
    value = field(record, payload, 0x0007, &type, &value_length);
    CHECK(value != NULL && type == 2 && value_length == 2 && le16(value) == 26);
    value = field(record, payload, 0x0020, &type, &value_length);
    CHECK(value != NULL && type == 4 && value_length == 4 && le32(value) == 150);
    value = field(record, payload, 0x0021, &type, &value_length);
    CHECK(value != NULL && type == 4 && value_length == 4 && le32(value) == 997);

    record = packet + ACTOR_OFFSET;
    for (i = 0; i < sizeof actors / sizeof actors[0]; ++i) {
        payload = le32(record + ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES);
        CHECK(le16(record + ZG_BRIDGE_RECORD_OFFSET_TYPE) == 0x0103);
        CHECK(le32(record + ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID) == actors[i].id);
        CHECK(check_string_field(record, payload, 0x0002, actors[i].name));
        value = field(record, payload, 0x0001, &type, &value_length);
        CHECK(value != NULL && type == 1 && value_length == 1 && value[0] == 3);
        value = field(record, payload, 0x0010, &type, &value_length);
        CHECK(value != NULL && type == 5 && lef32(value) == actors[i].x);
        value = field(record, payload, 0x0012, &type, &value_length);
        CHECK(value != NULL && type == 5 && lef32(value) == actors[i].z);
        value = field(record, payload, 0x0007, &type, &value_length);
        CHECK(value != NULL && type == 2 && le16(value) == actors[i].level);
        record += ZG_BRIDGE_RECORD_HEADER_BYTES + payload;
    }
    CHECK(record == packet + PACKET_BYTES);

    CHECK(zg_bridge_request_full(bridge) == ZG_BRIDGE_OK);
    length = 99;
    CHECK(zg_bridge_poll_render(bridge, packet, sizeof packet, &length) ==
        ZG_BRIDGE_OK);
    CHECK(length == PACKET_BYTES);
    CHECK(le32(packet + ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID) == 2);
    zg_bridge_get_stats(bridge, &stats);
    CHECK(stats.render_packets == 2);

    zg_bridge_destroy(bridge);
    printf("RENDER-PASS real map=2 self=烈火教父 npc=10 records=12 bytes=829\n");
    return 0;
}
