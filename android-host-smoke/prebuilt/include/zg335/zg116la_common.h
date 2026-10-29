#ifndef ZG116LA_COMMON_H
#define ZG116LA_COMMON_H

#include <limits.h>
#include <stdint.h>
#include "zg_normal116_fullstate.h"

#if CHAR_BIT != 8 || UINT32_MAX != 0xffffffffU
#error "zg116la requires eight-bit bytes and exact uint32_t"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ZG116LA_VERSION UINT32_C(2)

#define ZG116LA_SEAM_ALLOCATE UINT32_C(1)
#define ZG116LA_SEAM_FAILURE_IMPORT UINT32_C(2)
#define ZG116LA_SEAM_SLOT4 UINT32_C(3)
#define ZG116LA_SEAM_SLOT8 UINT32_C(4)
#define ZG116LA_SEAM_FINISH_NORMAL UINT32_C(5)
#define ZG116LA_SEAM_LENGTH_ERROR UINT32_C(6)

#define ZG116LA_REASON_NONE UINT32_C(0)
#define ZG116LA_REASON_BAD_INTERFACE UINT32_C(1)
#define ZG116LA_REASON_REQUEST_IDENTITY UINT32_C(2)
#define ZG116LA_REASON_READ_REPLY UINT32_C(3)
#define ZG116LA_REASON_WRITE_REPLY UINT32_C(4)
#define ZG116LA_REASON_MACHINE_REPLY UINT32_C(5)
#define ZG116LA_REASON_CHILD_RESULT UINT32_C(6)

typedef struct zg116la_identity {
    uint32_t wrapper_entry32;
    uint32_t ordinal32;
    uint32_t role32;
    uint32_t call_pc32;
    uint32_t target32;
    uint32_t continuation32;
} zg116la_identity;

/* One invocation of an actual external seam. entry_present is exactly0/1.
 * When1, entry/XMM are the actual callee-entry image after already-materialized
 * argument/return-PC stores; a fixed continuation requires entry_present1.
 * Legacy payload slot seams use0 because they have no actual slot-entry all-GPR
 * evidence; those entry fields are then not semantically consumed. Entry0 never
 * relaxes the required actual all8GPR/XMM0/returned_to successful reply.
 * Returned registers describe the actual post-RET image, before any caller
 * instruction such as ADD ESP,4. No guest word is a host pointer.
 */
typedef struct zg116la_machine_call {
    uint32_t version32;
    uint32_t seam32;
    uint32_t ordinal32;
    uint32_t target32;
    uint32_t call_pc32;
    uint32_t continuation_present;
    uint32_t continuation32;
    uint32_t request_bytes32;
    uint32_t object32;
    uint32_t fresh_table32;
    uint32_t fresh_target32;
    uint32_t source32;
    uint32_t message32;
    uint32_t core_return_value32;
    uint32_t callee_ret_cleanup32;
    uint32_t entry_present;
    zg116_regs8 entry;
    zg116_xmm0 xmm0_entry;
} zg116la_machine_call;

/* state32 uses ZG116_SERVICE_*. On OK all presence words and the normal ABI
 * premise are exactly1. STOPPED/INVALID/UNKNOWN need no success-only fields.
 * returned_to32 is the fresh logical target observed by the seam, not a host
 * function pointer. Actual ESP is returned.esp, including callee RET cleanup.
 */
typedef struct zg116la_machine_reply {
    uint32_t state32;
    uint32_t reason32;
    uint32_t normal_abi_preserved;
    uint32_t regs_present;
    zg116_regs8 returned;
    uint32_t xmm0_present;
    zg116_xmm0 xmm0_returned;
    uint32_t returned_to_present;
    uint32_t returned_to32;
} zg116la_machine_reply;

typedef void (*zg116la_machine_fn)(void *user,
    zg116la_machine_call request, zg116la_machine_reply *reply);

typedef struct zg116la_config {
    uint32_t version32;
    void *machine_user;
    zg116la_machine_fn machine_call;
} zg116la_config;

/* Package-internal invocation snapshot. Guest RAM/FS and userdata stay live;
 * only host tables, contexts and logical machine state are value-copied.
 */
typedef struct zg116la_bridge {
    zg116la_config config;
    zg116_memory memory;
    zg116_regs8 regs;
    zg116_xmm0 xmm0;
    uint32_t stopped_state32;
    uint32_t reason32;
} zg116la_bridge;

void zg116la_lower_clear(zg116_lower_reply *reply);
int zg116la_request_matches(const zg116_call *request,
    const zg116la_identity *identity);
int zg116la_bridge_begin(zg116la_bridge *bridge,
    const zg116la_config *config, const zg116_memory *memory,
    const zg116_call *request);
uint32_t zg116la_bridge_status(const zg116la_bridge *bridge);
void zg116la_bridge_latch(zg116la_bridge *bridge,
    uint32_t state32, uint32_t reason32);
void zg116la_lower_from_bridge(const zg116la_bridge *bridge,
    zg116_lower_reply *reply);
void zg116la_lower_returned(zg116_lower_reply *reply,
    zg116_regs8 regs, zg116_xmm0 xmm0,
    uint32_t returned_to32, uint32_t ret_cleanup32);

uint32_t zg116la_read32(zg116la_bridge *bridge, uint32_t space32,
    uint32_t address32, uint32_t *word_out);
uint32_t zg116la_write32(zg116la_bridge *bridge, uint32_t space32,
    uint32_t address32, uint32_t word32);
uint32_t zg116la_write8(zg116la_bridge *bridge, uint32_t space32,
    uint32_t address32, uint32_t low_byte);

/* Invoke the external seam exactly once. reply_out receives its entire raw
 * logical reply, including malformed/stopped replies. The return value is the
 * normalized ZG116_SERVICE_* state. On OK, all combined evidence is validated
 * before bridge regs/XMM are replaced. Missing evidence latches UNKNOWN and is
 * never repaired by copying the entry image or issuing a second invocation.
 */
uint32_t zg116la_invoke_machine(zg116la_bridge *bridge,
    zg116la_machine_call request, zg116la_machine_reply *reply_out);

#ifdef __cplusplus
}
#endif
#endif
