#ifndef ZG116LA_RAW52_H
#define ZG116LA_RAW52_H

#include "zg116la_common.h"
#include "zg_node_provider52_candidate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Complete portable normal-path envelopes for the two verified 52-byte
 * provider bodies. The unchanged provider core owns allocation/node logic;
 * this layer owns actual stack, import, register and fresh RET8 boundaries.
 * Native CRT/SEH/allocation ownership remain outside this interface.
 */
void zg116la_fe0_sentinel(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply);
void zg116la_e4c0_sentinel(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply);

#ifdef __cplusplus
}
#endif
#endif
