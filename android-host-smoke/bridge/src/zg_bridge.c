#include "zg_bridge.h"

#include <stdlib.h>
#include <string.h>

struct zg_bridge {
    zg_bridge_config config;
    uint8_t *rx;
    size_t rx_head;
    size_t rx_count;
    uint8_t *tx;
    size_t tx_head;
    size_t tx_count;
    uint32_t request_full_pending;
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
    bridge->rx = (uint8_t *)malloc(config->max_frame_bytes);
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
    (void)now_ms;

    if (bridge == NULL || (bytes == NULL && length != 0)) {
        set_error(bridge, ZG_BRIDGE_PROTOCOL_ERROR);
        return ZG_BRIDGE_PROTOCOL_ERROR;
    }
    if (length > bridge->config.max_frame_bytes) {
        set_error(bridge, ZG_BRIDGE_OVERSIZE);
        return ZG_BRIDGE_OVERSIZE;
    }
    if (length > ring_free(bridge->config.max_frame_bytes, bridge->rx_count)) {
        set_error(bridge, ZG_BRIDGE_BACKPRESSURE);
        return ZG_BRIDGE_BACKPRESSURE;
    }
    if (length == 0) {
        return ZG_BRIDGE_OK;
    }
    ring_append(bridge->rx, bridge->config.max_frame_bytes, &bridge->rx_head,
        &bridge->rx_count, bytes, length);
    bridge->stats.rx_bytes += (uint64_t)length;
    bridge->stats.frames += 1;
    return ZG_BRIDGE_OK;
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
    /* Phase 0 deliberately has no render producer. */
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
