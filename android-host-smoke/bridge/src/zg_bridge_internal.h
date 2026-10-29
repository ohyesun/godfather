#ifndef ZG_BRIDGE_INTERNAL_H
#define ZG_BRIDGE_INTERNAL_H

#include "zg_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Test/diagnostic-only JSON counts. Not part of the wire ABI or
 * zg_bridge_stats. */
typedef struct zg_bridge_internal_json_stats {
    uint32_t tokens;
    uint32_t keys;
} zg_bridge_internal_json_stats;

void zg_bridge_internal_get_json_stats(const zg_bridge *bridge,
    zg_bridge_internal_json_stats *out);

#ifdef __cplusplus
}
#endif

#endif /* ZG_BRIDGE_INTERNAL_H */
