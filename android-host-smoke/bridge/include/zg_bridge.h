#ifndef ZG_BRIDGE_H
#define ZG_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZG_BRIDGE_ABI_VERSION UINT32_C(1)
#define ZG_BRIDGE_MAGIC UINT16_C(0x5a47) /* 'ZG', little-endian wire bytes 47 5a */
#define ZG_BRIDGE_ENVELOPE_VERSION UINT8_C(1)
#define ZG_BRIDGE_ENVELOPE_HEADER_BYTES UINT16_C(24)

/* Envelope kind values. */
#define ZG_BRIDGE_KIND_FULL UINT8_C(1)
#define ZG_BRIDGE_KIND_DELTA UINT8_C(2)
#define ZG_BRIDGE_KIND_CONTROL UINT8_C(3)
#define ZG_BRIDGE_KIND_ERROR UINT8_C(4)

/* Envelope flags. */
#define ZG_BRIDGE_FLAG_KEYFRAME UINT16_C(0x0001)
#define ZG_BRIDGE_FLAG_RESYNC UINT16_C(0x0002)
#define ZG_BRIDGE_FLAG_RESERVED_MASK UINT16_C(0xfffc)

/* 24-byte little-endian envelope, offsets frozen by the spec. */
#define ZG_BRIDGE_ENVELOPE_OFFSET_MAGIC UINT16_C(0)
#define ZG_BRIDGE_ENVELOPE_OFFSET_VERSION UINT16_C(2)
#define ZG_BRIDGE_ENVELOPE_OFFSET_KIND UINT16_C(3)
#define ZG_BRIDGE_ENVELOPE_OFFSET_FLAGS UINT16_C(4)
#define ZG_BRIDGE_ENVELOPE_OFFSET_HEADER_BYTES UINT16_C(6)
#define ZG_BRIDGE_ENVELOPE_OFFSET_FRAME_ID UINT16_C(8)
#define ZG_BRIDGE_ENVELOPE_OFFSET_BASE_FRAME_ID UINT16_C(12)
#define ZG_BRIDGE_ENVELOPE_OFFSET_PAYLOAD_BYTES UINT16_C(16)
#define ZG_BRIDGE_ENVELOPE_OFFSET_RECORD_COUNT UINT16_C(20)
#define ZG_BRIDGE_ENVELOPE_OFFSET_RESERVED UINT16_C(22)
#define ZG_BRIDGE_ENVELOPE_SIZE UINT16_C(24)

/* Record framing is frozen; game field payloads are not. */
#define ZG_BRIDGE_RECORD_HEADER_BYTES UINT16_C(12)
#define ZG_BRIDGE_RECORD_OFFSET_TYPE UINT16_C(0)
#define ZG_BRIDGE_RECORD_OFFSET_SCHEMA UINT16_C(2)
#define ZG_BRIDGE_RECORD_OFFSET_ENTITY_ID UINT16_C(4)
#define ZG_BRIDGE_RECORD_OFFSET_PAYLOAD_BYTES UINT16_C(8)

#define ZG_BRIDGE_MAX_LIMIT_BYTES (UINT32_C(16) * UINT32_C(1024) * UINT32_C(1024))

typedef struct zg_bridge zg_bridge;

typedef struct zg_bridge_config {
    uint32_t abi_version;
    uint32_t max_frame_bytes;
    uint32_t max_packet_bytes;
    uint32_t max_tx_bytes;
    uint32_t flags;
} zg_bridge_config;

typedef enum zg_bridge_status {
    ZG_BRIDGE_OK = 0,
    ZG_BRIDGE_NEED_MORE = 1,
    ZG_BRIDGE_AGAIN = 2,
    ZG_BRIDGE_RESYNC_REQUIRED = 3,
    ZG_BRIDGE_PROTOCOL_ERROR = 4,
    ZG_BRIDGE_OVERSIZE = 5,
    ZG_BRIDGE_BACKPRESSURE = 6,
    ZG_BRIDGE_AUTH_REQUIRED = 7,
    ZG_BRIDGE_CLOSED = 8
} zg_bridge_status;

typedef struct zg_bridge_stats {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t frames;       /* Phase 0: accepted non-empty feed chunks */
    uint64_t render_packets;
    uint64_t render_dropped;
    uint64_t decode_ns;
    uint32_t render_queue_depth;
    uint32_t tx_queue_depth;
    uint32_t last_error;
} zg_bridge_stats;

/* Create validates ABI version, non-zero bounded limits, and reserved flags. */
zg_bridge *zg_bridge_create(const zg_bridge_config *config);
void zg_bridge_destroy(zg_bridge *bridge);

/* Feed raw transport bytes. Phase 0 appends bounded bytes; it does not parse
 * game commands or create frames. */
zg_bridge_status zg_bridge_feed(
    zg_bridge *bridge,
    const uint8_t *bytes,
    size_t length,
    uint64_t now_ms);

/* Nonblocking render poll. Phase 0 has no render producer, so an empty queue
 * returns ZG_BRIDGE_AGAIN. */
zg_bridge_status zg_bridge_poll_render(
    zg_bridge *bridge,
    uint8_t *dst,
    size_t capacity,
    size_t *out_length);

/* Nonblocking outbound poll. Returns one queued byte chunk. */
zg_bridge_status zg_bridge_poll_tx(
    zg_bridge *bridge,
    uint8_t *dst,
    size_t capacity,
    size_t *out_length);

/* Queue binary input. Phase 0 only bounds/copies it; no game parsing. */
zg_bridge_status zg_bridge_queue_input(
    zg_bridge *bridge,
    const uint8_t *bytes,
    size_t length,
    uint64_t now_ms);

/* Mark a full-state request. Phase 0 records the request only. */
zg_bridge_status zg_bridge_request_full(zg_bridge *bridge);

void zg_bridge_get_stats(const zg_bridge *bridge, zg_bridge_stats *out);

#ifdef __cplusplus
}
#endif

#endif /* ZG_BRIDGE_H */
