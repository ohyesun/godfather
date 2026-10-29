#ifndef ZG116LA_E4_RANGE_H
#define ZG116LA_E4_RANGE_H

#include "zg116la_common.h"
#include "zg_range101e0_raw_frame.h"
#include "zg_wrapper154_candidate.h"
#include "zg_node_provider52_candidate.h"
#include "zg_node_copy32_candidate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* E4C0 0x100101e0 complete raw-frame binding. The frozen raw143/CD40/provider52
 * and payload175 kernels are unchanged; this file only supplies coherent memory,
 * machine replies, XMM0 and lower-state projection. Native ABI/CRT/SEH,
 * allocation ownership and physical atomicity are outside this SOURCE contract.
 */
void zg116la_e4_range(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply);

#ifdef __cplusplus
}
#endif
#endif
