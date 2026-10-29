#include "zg_bridge.h"
#include "zg_bridge_internal.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define ZG_FRAME_HEADER_BYTES UINT32_C(4)
#define ZG_FRAME_ABS_MAX_BYTES (UINT32_C(2) * UINT32_C(1024) * UINT32_C(1024))
#define ZG_CONTROL_BYTES UINT32_C(24)
#define ZG_JSON_MAX_DEPTH 64U

struct zg_bridge {
    zg_bridge_config config;
    size_t rx_capacity;
    uint8_t *rx;
    size_t rx_head;
    size_t rx_count;
    uint8_t *tx;
    size_t tx_head;
    size_t tx_count;
    uint32_t request_full_pending;
    uint64_t json_tokens;
    uint64_t json_keys;
    zg_bridge_stats stats;
};

static void set_error(zg_bridge *bridge, zg_bridge_status status)
{
    if (bridge != NULL) {
        bridge->stats.last_error = (uint32_t)status;
    }
}

static int config_valid(const zg_bridge_config *config)
{
    if (config == NULL || config->abi_version != ZG_BRIDGE_ABI_VERSION) {
        return 0;
    }
    if (config->flags != UINT32_C(0)) {
        return 0;
    }
    if (config->max_frame_bytes == 0 || config->max_packet_bytes == 0 ||
        config->max_tx_bytes == 0) {
        return 0;
    }
    if (config->max_frame_bytes > ZG_BRIDGE_MAX_LIMIT_BYTES ||
        config->max_packet_bytes > ZG_BRIDGE_MAX_LIMIT_BYTES ||
        config->max_tx_bytes > ZG_BRIDGE_MAX_LIMIT_BYTES) {
        return 0;
    }
    return 1;
}

static size_t ring_free(size_t capacity, size_t count)
{
    return capacity - count;
}

static void ring_append(uint8_t *ring, size_t capacity, size_t *head,
    size_t *count, const uint8_t *bytes, size_t length)
{
    size_t tail = (*head + *count) % capacity;
    size_t first = capacity - tail;
    if (first > length) {
        first = length;
    }
    memcpy(ring + tail, bytes, first);
    if (length > first) {
        memcpy(ring, bytes + first, length - first);
    }
    *count += length;
}

static uint8_t ring_byte(const zg_bridge *bridge, size_t offset)
{
    return bridge->rx[(bridge->rx_head + offset) % bridge->rx_capacity];
}

static void ring_copy(const zg_bridge *bridge, size_t offset, uint8_t *out,
    size_t length)
{
    size_t first = bridge->rx_capacity - ((bridge->rx_head + offset) %
        bridge->rx_capacity);
    if (first > length) {
        first = length;
    }
    memcpy(out, bridge->rx + ((bridge->rx_head + offset) % bridge->rx_capacity),
        first);
    if (length > first) {
        memcpy(out + first, bridge->rx, length - first);
    }
}

static void ring_drop(zg_bridge *bridge, size_t length)
{
    bridge->rx_head = (bridge->rx_head + length) % bridge->rx_capacity;
    bridge->rx_count -= length;
}

static void ring_clear(zg_bridge *bridge)
{
    bridge->rx_head = 0;
    bridge->rx_count = 0;
}

static uint32_t peek_be32(const zg_bridge *bridge)
{
    return ((uint32_t)ring_byte(bridge, 0) << 24) |
        ((uint32_t)ring_byte(bridge, 1) << 16) |
        ((uint32_t)ring_byte(bridge, 2) << 8) |
        (uint32_t)ring_byte(bridge, 3);
}

static uint16_t peek_le16(const zg_bridge *bridge, size_t offset)
{
    return (uint16_t)(ring_byte(bridge, offset) |
        ((uint16_t)ring_byte(bridge, offset + 1) << 8));
}

static uint32_t peek_le32(const zg_bridge *bridge, size_t offset)
{
    return (uint32_t)ring_byte(bridge, offset) |
        ((uint32_t)ring_byte(bridge, offset + 1) << 8) |
        ((uint32_t)ring_byte(bridge, offset + 2) << 16) |
        ((uint32_t)ring_byte(bridge, offset + 3) << 24);
}

static int is_control_prefix(const zg_bridge *bridge)
{
    return ring_byte(bridge, 0) == UINT8_C(0x47) &&
        ring_byte(bridge, 1) == UINT8_C(0x5a) &&
        ring_byte(bridge, 2) == ZG_BRIDGE_ENVELOPE_VERSION &&
        ring_byte(bridge, 3) == ZG_BRIDGE_KIND_CONTROL;
}

static int is_control_frame(const zg_bridge *bridge)
{
    return bridge->rx_count >= ZG_CONTROL_BYTES &&
        is_control_prefix(bridge) &&
        peek_le16(bridge, 6) == ZG_BRIDGE_ENVELOPE_HEADER_BYTES &&
        peek_le32(bridge, 16) == 0 &&
        peek_le16(bridge, 20) == 0;
}

typedef struct JsonCursor {
    const uint8_t *bytes;
    size_t length;
    size_t offset;
    uint32_t tokens;
    uint32_t keys;
} JsonCursor;

static void json_skip_ws(JsonCursor *cursor)
{
    while (cursor->offset < cursor->length) {
        uint8_t value = cursor->bytes[cursor->offset];
        if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
            break;
        }
        ++cursor->offset;
    }
}

static int json_string(JsonCursor *cursor, int is_key)
{
    if (cursor->offset >= cursor->length ||
        cursor->bytes[cursor->offset] != '"') {
        return 0;
    }
    ++cursor->offset;
    ++cursor->tokens;
    while (cursor->offset < cursor->length) {
        uint8_t value = cursor->bytes[cursor->offset++];
        if (value == '"') {
            if (is_key) {
                ++cursor->keys;
            }
            return 1;
        }
        if (value == '\\') {
            if (cursor->offset >= cursor->length) {
                return 0;
            }
            ++cursor->offset;
        } else if (value < 0x20) {
            return 0;
        }
    }
    return 0;
}

static int json_literal(JsonCursor *cursor, const char *literal)
{
    size_t length = strlen(literal);
    if (cursor->offset + length > cursor->length ||
        memcmp(cursor->bytes + cursor->offset, literal, length) != 0) {
        return 0;
    }
    cursor->offset += length;
    ++cursor->tokens;
    return 1;
}

static int json_value(JsonCursor *cursor, unsigned depth);

static int json_number(JsonCursor *cursor)
{
    size_t start = cursor->offset;
    if (cursor->offset < cursor->length &&
        cursor->bytes[cursor->offset] == '-') {
        ++cursor->offset;
    }
    if (cursor->offset >= cursor->length ||
        cursor->bytes[cursor->offset] < '0' ||
        cursor->bytes[cursor->offset] > '9') {
        return 0;
    }
    if (cursor->bytes[cursor->offset] == '0') {
        ++cursor->offset;
    } else {
        while (cursor->offset < cursor->length &&
            cursor->bytes[cursor->offset] >= '0' &&
            cursor->bytes[cursor->offset] <= '9') {
            ++cursor->offset;
        }
    }
    if (cursor->offset < cursor->length &&
        cursor->bytes[cursor->offset] == '.') {
        ++cursor->offset;
        if (cursor->offset >= cursor->length ||
            cursor->bytes[cursor->offset] < '0' ||
            cursor->bytes[cursor->offset] > '9') {
            return 0;
        }
        while (cursor->offset < cursor->length &&
            cursor->bytes[cursor->offset] >= '0' &&
            cursor->bytes[cursor->offset] <= '9') {
            ++cursor->offset;
        }
    }
    if (cursor->offset < cursor->length &&
        (cursor->bytes[cursor->offset] == 'e' ||
            cursor->bytes[cursor->offset] == 'E')) {
        ++cursor->offset;
        if (cursor->offset < cursor->length &&
            (cursor->bytes[cursor->offset] == '+' ||
                cursor->bytes[cursor->offset] == '-')) {
            ++cursor->offset;
        }
        if (cursor->offset >= cursor->length ||
            cursor->bytes[cursor->offset] < '0' ||
            cursor->bytes[cursor->offset] > '9') {
            return 0;
        }
        while (cursor->offset < cursor->length &&
            cursor->bytes[cursor->offset] >= '0' &&
            cursor->bytes[cursor->offset] <= '9') {
            ++cursor->offset;
        }
    }
    if (cursor->offset == start) {
        return 0;
    }
    ++cursor->tokens;
    return 1;
}

static int json_object(JsonCursor *cursor, unsigned depth)
{
    if (depth >= ZG_JSON_MAX_DEPTH) {
        return 0;
    }
    ++cursor->offset;
    ++cursor->tokens;
    json_skip_ws(cursor);
    if (cursor->offset < cursor->length &&
        cursor->bytes[cursor->offset] == '}') {
        ++cursor->offset;
        return 1;
    }
    for (;;) {
        json_skip_ws(cursor);
        if (!json_string(cursor, 1)) {
            return 0;
        }
        json_skip_ws(cursor);
        if (cursor->offset >= cursor->length ||
            cursor->bytes[cursor->offset] != ':') {
            return 0;
        }
        ++cursor->offset;
        if (!json_value(cursor, depth + 1)) {
            return 0;
        }
        json_skip_ws(cursor);
        if (cursor->offset >= cursor->length) {
            return 0;
        }
        if (cursor->bytes[cursor->offset] == ',') {
            ++cursor->offset;
            continue;
        }
        if (cursor->bytes[cursor->offset] == '}') {
            ++cursor->offset;
            return 1;
        }
        return 0;
    }
}

static int json_array(JsonCursor *cursor, unsigned depth)
{
    if (depth >= ZG_JSON_MAX_DEPTH) {
        return 0;
    }
    ++cursor->offset;
    ++cursor->tokens;
    json_skip_ws(cursor);
    if (cursor->offset < cursor->length &&
        cursor->bytes[cursor->offset] == ']') {
        ++cursor->offset;
        return 1;
    }
    for (;;) {
        if (!json_value(cursor, depth + 1)) {
            return 0;
        }
        json_skip_ws(cursor);
        if (cursor->offset >= cursor->length) {
            return 0;
        }
        if (cursor->bytes[cursor->offset] == ',') {
            ++cursor->offset;
            continue;
        }
        if (cursor->bytes[cursor->offset] == ']') {
            ++cursor->offset;
            return 1;
        }
        return 0;
    }
}

static int json_value(JsonCursor *cursor, unsigned depth)
{
    json_skip_ws(cursor);
    if (cursor->offset >= cursor->length) {
        return 0;
    }
    switch (cursor->bytes[cursor->offset]) {
    case '{':
        return json_object(cursor, depth);
    case '[':
        return json_array(cursor, depth);
    case '"':
        return json_string(cursor, 0);
    case 't':
        return json_literal(cursor, "true");
    case 'f':
        return json_literal(cursor, "false");
    case 'n':
        return json_literal(cursor, "null");
    default:
        return json_number(cursor);
    }
}

static int json_count(const uint8_t *bytes, size_t length, uint32_t *tokens,
    uint32_t *keys)
{
    JsonCursor cursor;
    memset(&cursor, 0, sizeof cursor);
    cursor.bytes = bytes;
    cursor.length = length;
    if (!json_value(&cursor, 0)) {
        return 0;
    }
    json_skip_ws(&cursor);
    if (cursor.offset != length) {
        return 0;
    }
    *tokens = cursor.tokens;
    *keys = cursor.keys;
    return 1;
}

static zg_bridge_status decode_gzip_json(zg_bridge *bridge,
    const uint8_t *body, size_t length)
{
    z_stream stream;
    uint8_t *plain;
    size_t plain_capacity;
    size_t plain_length;
    uint32_t tokens;
    uint32_t keys;
    int result;

    plain_capacity = bridge->config.max_packet_bytes;
    if (plain_capacity > ZG_FRAME_ABS_MAX_BYTES) {
        plain_capacity = ZG_FRAME_ABS_MAX_BYTES;
    }
    plain = (uint8_t *)malloc(plain_capacity);
    if (plain == NULL) {
        set_error(bridge, ZG_BRIDGE_BACKPRESSURE);
        return ZG_BRIDGE_BACKPRESSURE;
    }
    memset(&stream, 0, sizeof stream);
    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        free(plain);
        set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
        return ZG_BRIDGE_PROTOCOL_ERROR;
    }
    stream.next_in = (Bytef *)body;
    stream.avail_in = (uInt)length;
    stream.next_out = plain;
    stream.avail_out = (uInt)plain_capacity;
    result = inflate(&stream, Z_FINISH);
    plain_length = plain_capacity - stream.avail_out;
    inflateEnd(&stream);
    if (result != Z_STREAM_END) {
        free(plain);
        set_error(bridge, result == Z_BUF_ERROR && plain_length == plain_capacity ?
            ZG_BRIDGE_OVERSIZE : ZG_BRIDGE_PROTOCOL_ERROR);
        return bridge->stats.last_error == ZG_BRIDGE_OVERSIZE ?
            ZG_BRIDGE_OVERSIZE : ZG_BRIDGE_PROTOCOL_ERROR;
    }
    if (!json_count(plain, plain_length, &tokens, &keys)) {
        free(plain);
        set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
        return ZG_BRIDGE_PROTOCOL_ERROR;
    }
    free(plain);
    bridge->json_tokens += tokens;
    bridge->json_keys += keys;
    return ZG_BRIDGE_OK;
}

static zg_bridge_status process_rx(zg_bridge *bridge)
{
    for (;;) {
        uint32_t length;

        if (bridge->rx_count < ZG_FRAME_HEADER_BYTES) {
            return ZG_BRIDGE_OK;
        }
        if (is_control_prefix(bridge)) {
            if (bridge->rx_count < ZG_CONTROL_BYTES) {
                return ZG_BRIDGE_OK;
            }
            if (is_control_frame(bridge)) {
                ring_drop(bridge, ZG_CONTROL_BYTES);
                ++bridge->stats.frames;
                continue;
            }
        }
        length = peek_be32(bridge);
        if (length == 0) {
            set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
            ring_clear(bridge);
            return ZG_BRIDGE_PROTOCOL_ERROR;
        }
        if (length > ZG_FRAME_ABS_MAX_BYTES ||
            length > bridge->config.max_frame_bytes) {
            set_error(bridge, ZG_BRIDGE_OVERSIZE);
            ring_clear(bridge);
            return ZG_BRIDGE_OVERSIZE;
        }
        if (bridge->rx_count < (size_t)length + ZG_FRAME_HEADER_BYTES) {
            return ZG_BRIDGE_OK;
        }
        {
            uint8_t *body = (uint8_t *)malloc(length);
            zg_bridge_status status;
            if (body == NULL) {
                set_error(bridge, ZG_BRIDGE_BACKPRESSURE);
                ring_clear(bridge);
                return ZG_BRIDGE_BACKPRESSURE;
            }
            ring_copy(bridge, ZG_FRAME_HEADER_BYTES, body, length);
            ring_drop(bridge, (size_t)length + ZG_FRAME_HEADER_BYTES);
            status = decode_gzip_json(bridge, body, length);
            free(body);
            if (status != ZG_BRIDGE_OK) {
                ring_clear(bridge);
                return status;
            }
            ++bridge->stats.frames;
        }
    }
}

zg_bridge *zg_bridge_create(const zg_bridge_config *config)
{
    zg_bridge *bridge;

    if (!config_valid(config)) {
        return NULL;
    }
    bridge = (zg_bridge *)calloc(1, sizeof(*bridge));
    if (bridge == NULL) {
        return NULL;
    }
    bridge->rx_capacity = (size_t)config->max_frame_bytes +
        ZG_FRAME_HEADER_BYTES;
    bridge->rx = (uint8_t *)malloc(bridge->rx_capacity);
    bridge->tx = (uint8_t *)malloc(config->max_tx_bytes);
    if (bridge->rx == NULL || bridge->tx == NULL) {
        free(bridge->rx);
        free(bridge->tx);
        free(bridge);
        return NULL;
    }
    bridge->config = *config;
    return bridge;
}

void zg_bridge_destroy(zg_bridge *bridge)
{
    if (bridge == NULL) {
        return;
    }
    free(bridge->rx);
    free(bridge->tx);
    free(bridge);
}

zg_bridge_status zg_bridge_feed(
    zg_bridge *bridge,
    const uint8_t *bytes,
    size_t length,
    uint64_t now_ms)
{
    zg_bridge_status status;

    (void)now_ms;

    if (bridge == NULL || (bytes == NULL && length != 0)) {
        set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
        return ZG_BRIDGE_PROTOCOL_ERROR;
    }
    status = process_rx(bridge);
    if (status != ZG_BRIDGE_OK) {
        return status;
    }
    if (length > bridge->rx_capacity ||
        length > ring_free(bridge->rx_capacity, bridge->rx_count)) {
        set_error(bridge, length > bridge->rx_capacity ?
            ZG_BRIDGE_OVERSIZE : ZG_BRIDGE_BACKPRESSURE);
        return bridge->stats.last_error == ZG_BRIDGE_OVERSIZE ?
            ZG_BRIDGE_OVERSIZE : ZG_BRIDGE_BACKPRESSURE;
    }
    if (length != 0) {
        ring_append(bridge->rx, bridge->rx_capacity, &bridge->rx_head,
            &bridge->rx_count, bytes, length);
        bridge->stats.rx_bytes += (uint64_t)length;
    }
    return process_rx(bridge);
}

zg_bridge_status zg_bridge_poll_render(
    zg_bridge *bridge,
    uint8_t *dst,
    size_t capacity,
    size_t *out_length)
{
    if (bridge == NULL || out_length == NULL ||
        (dst == NULL && capacity != 0)) {
        set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
        return ZG_BRIDGE_PROTOCOL_ERROR;
    }
    *out_length = 0;
    return ZG_BRIDGE_AGAIN;
}

zg_bridge_status zg_bridge_poll_tx(
    zg_bridge *bridge,
    uint8_t *dst,
    size_t capacity,
    size_t *out_length)
{
    size_t length;
    size_t first;

    if (bridge == NULL || out_length == NULL ||
        (dst == NULL && capacity != 0)) {
        set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
        return ZG_BRIDGE_PROTOCOL_ERROR;
    }
    *out_length = 0;
    if (bridge->tx_count == 0) {
        return ZG_BRIDGE_AGAIN;
    }
    length = bridge->tx_count;
    if (capacity < length) {
        set_error(bridge, ZG_BRIDGE_BACKPRESSURE);
        return ZG_BRIDGE_BACKPRESSURE;
    }
    first = bridge->config.max_tx_bytes - bridge->tx_head;
    if (first > length) {
        first = length;
    }
    memcpy(dst, bridge->tx + bridge->tx_head, first);
    if (length > first) {
        memcpy(dst + first, bridge->tx, length - first);
    }
    bridge->tx_head = (bridge->tx_head + length) % bridge->config.max_tx_bytes;
    bridge->tx_count = 0;
    *out_length = length;
    bridge->stats.tx_queue_depth = 0;
    return ZG_BRIDGE_OK;
}

zg_bridge_status zg_bridge_queue_input(
    zg_bridge *bridge,
    const uint8_t *bytes,
    size_t length,
    uint64_t now_ms)
{
    (void)now_ms;

    if (bridge == NULL || (bytes == NULL && length != 0)) {
        set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
        return ZG_BRIDGE_PROTOCOL_ERROR;
    }
    if (length > bridge->config.max_tx_bytes) {
        set_error(bridge, ZG_BRIDGE_OVERSIZE);
        return ZG_BRIDGE_OVERSIZE;
    }
    if (length > ring_free(bridge->config.max_tx_bytes, bridge->tx_count)) {
        set_error(bridge, ZG_BRIDGE_BACKPRESSURE);
        return ZG_BRIDGE_BACKPRESSURE;
    }
    if (length == 0) {
        return ZG_BRIDGE_OK;
    }
    ring_append(bridge->tx, bridge->config.max_tx_bytes, &bridge->tx_head,
        &bridge->tx_count, bytes, length);
    bridge->stats.tx_bytes += (uint64_t)length;
    bridge->stats.tx_queue_depth = (uint32_t)bridge->tx_count;
    return ZG_BRIDGE_OK;
}

zg_bridge_status zg_bridge_request_full(zg_bridge *bridge)
{
    if (bridge == NULL) {
        set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
        return ZG_BRIDGE_PROTOCOL_ERROR;
    }
    bridge->request_full_pending = UINT32_C(1);
    return ZG_BRIDGE_OK;
}

void zg_bridge_get_stats(const zg_bridge *bridge, zg_bridge_stats *out)
{
    if (out == NULL) {
        return;
    }
    if (bridge == NULL) {
        memset(out, 0, sizeof(*out));
        return;
    }
    *out = bridge->stats;
    out->render_queue_depth = 0;
    out->tx_queue_depth = (uint32_t)bridge->tx_count;
}

void zg_bridge_internal_get_json_stats(const zg_bridge *bridge,
    zg_bridge_internal_json_stats *out)
{
    if (out == NULL) {
        return;
    }
    if (bridge == NULL) {
        memset(out, 0, sizeof(*out));
        return;
    }
    out->tokens = (uint32_t)bridge->json_tokens;
    out->keys = (uint32_t)bridge->json_keys;
}
