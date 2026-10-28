#ifndef ZG116LA_FE0_RANGE_H
#define ZG116LA_FE0_RANGE_H

#include "zg116la_common.h"
#include "zg_range76e0.h"
#include "zg_wrapper154_candidate.h"
#include "zg_node_provider52_candidate.h"
#include "zg_node_copy32_candidate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FE0 0x100076e0 complete portable normal-path lower callback. The outer
 * prologue/epilogue and callback-RAM composition are new; the four algorithmic
 * kernels are called unchanged. Physical atomicity, native ABI/CRT/SEH and
 * allocation ownership are not claimed by this interface.
 */
void zg116la_fe0_range(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply);

#ifdef __cplusplus
}
#endif
#endif
