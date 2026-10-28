#ifndef ZG_WRAPPER154_CANDIDATE_H
#define ZG_WRAPPER154_CANDIDATE_H

#include <stdint.h>

#if UINT32_MAX != 0xffffffffu
#error "The wrapper154 candidate requires exact uint32_t."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Prospective SOURCE443 source, not an acceptance or native execution API. */
typedef enum zg_w154_state {
    ZG_W154_RETURNED = 0,
    ZG_W154_NONRETURN = 1,
    ZG_W154_ERROR = 2,
    ZG_W154_UNKNOWN = 3
} zg_w154_state;

typedef enum zg_w154_reason {
    ZG_W154_REASON_NONE = 0,
    ZG_W154_REASON_BAD_INTERFACE = 1,
    ZG_W154_REASON_SERVICE_STOP = 2,
    ZG_W154_REASON_INVALID_STATE = 3,
    ZG_W154_REASON_ABI_UNCONFIRMED = 4,
    ZG_W154_REASON_ABI_MISMATCH = 5
} zg_w154_reason;

typedef enum zg_w154_variant {
    ZG_W154_CD40 = 0,
    ZG_W154_75E0 = 1
} zg_w154_variant;

typedef enum zg_w154_phase {
    ZG_W154_PHASE_INTERFACE = 0,
    ZG_W154_PHASE_READ32 = 1,
    ZG_W154_PHASE_WRITE32 = 2,
    ZG_W154_PHASE_WRITE8 = 3,
    ZG_W154_PHASE_READ_FS_HEAD = 4,
    ZG_W154_PHASE_WRITE_FS_HEAD = 5,
    ZG_W154_PHASE_CALLEE = 6,
    ZG_W154_PHASE_DONE = 7
} zg_w154_phase;

typedef struct zg_w154_regs {
    uint32_t eax32, ecx32, edx32, ebx32;
    uint32_t esp32, ebp32, esi32, edi32;
} zg_w154_regs;

/* Each logical memory word is little endian over four wrapping byte addresses.
 * write8 changes exactly one byte. read32/write32 must share the SAME backing
 * bytes for stack, argument, frame and object accesses, including overlaps and
 * address zero. Separate FS-head capabilities may alias this backing too.
 * RETURNED requires a valid read output. A stopping service may have effects.
 * Hooks preserve logical registers; only a callee reply supplies new registers.
 */
typedef zg_w154_state (*zg_w154_read_fn)(
    void *context, uint32_t address32, uint32_t *word_out);
typedef zg_w154_state (*zg_w154_write_fn)(
    void *context, uint32_t address32, uint32_t word);
typedef zg_w154_state (*zg_w154_write8_fn)(
    void *context, uint32_t address32, uint32_t low_byte);
typedef zg_w154_state (*zg_w154_fs_read_fn)(void *context, uint32_t *word_out);
typedef zg_w154_state (*zg_w154_fs_write_fn)(void *context, uint32_t word);

typedef struct zg_w154_call {
    uint32_t ordinal32;                /* 1 = provider52, 2 = copy175 */
    uint32_t target32;                 /* body address for this variant */
    uint32_t continuation32;           /* expected normal continuation */
    uint32_t callee_ret_cleanup32;     /* 8 for first; 4 for second */
    zg_w154_regs entry;                /* AFTER CALL's return-PC store */
} zg_w154_call;

typedef struct zg_w154_call_result {
    zg_w154_state state;
    /* On RETURNED all registers and returned_to32 are valid. They describe
     * the supplied normal callee return AFTER its RET8/RET4 cleanup. The hook
     * owns callee-internal memory effects, stack scratch and return-slot read.
     * Exactly1 is an explicit adapter premise, not a native ABI observation.
     * The candidate checks it, the actual continuation, ESP and nonvolatiles.
     * It cannot check omitted callee internals or prove a native return.
     */
    uint32_t normal_abi_preserved;
    uint32_t returned_to32;
    zg_w154_regs returned;
} zg_w154_call_result;

typedef zg_w154_call_result (*zg_w154_callee_fn)(
    void *context, zg_w154_call call);

typedef struct zg_w154_services {
    void *context;
    zg_w154_read_fn read32;
    zg_w154_write_fn write32;
    zg_w154_write8_fn write8;
    zg_w154_fs_read_fn read_fs_head32;
    zg_w154_fs_write_fn write_fs_head32;
    zg_w154_callee_fn call_first;
    zg_w154_callee_fn call_second;
} zg_w154_services;

typedef struct zg_w154_result {
    zg_w154_state state;
    zg_w154_reason reason;
    uint32_t instruction_offset32;     /* offset in either 154-byte body */
    zg_w154_phase phase;
    zg_w154_regs registers32;
    /* These three words are valid only on RETURNED/PHASE_DONE. The fresh
     * logical RET12 target is reported, never executed or converted to a
     * host pointer. registers32.esp32 includes pop return-PC plus 12 bytes.
     */
    uint32_t return_value32;
    uint32_t return_target32;
    uint32_t wrapper_ret_cleanup32;
} zg_w154_result;

/* entry.esp32 addresses the caller's existing return PC; args are entry.esp32
 * +4/+8/+12. No argument is captured at API entry. All arithmetic wraps32.
 * All seven hooks/context are copied and checked before the first event;
 * missing hooks and invalid variants return ERROR with no service effects.
 * This interface snapshot and all stop/ABI checks are NEW PORTABLE POLICY.
 *
 * NONRETURN/UNKNOWN/ERROR record a committed prefix; no unwind, cleanup,
 * rollback or retry occurs. An exception/fault/unknown ABI must be reported by
 * a hook as UNKNOWN (or a deliberate service ERROR); no SEH is synthesized.
 * On a non-RETURNED callee reply, registers32 remains the last defined wrapper
 * snapshot at callee entry; opaque endpoint registers are not inferred.
 * Trusted live hook/output storage and stable invocation capabilities are
 * caller premises. Null context is allowed. Logical memory is not host RAM.
 * No concurrency, physical stack/FS, flags, CRT, lifetime or ownership claim.
 */
zg_w154_result zg_wrapper154_candidate(
    const zg_w154_services *services,
    zg_w154_variant variant, zg_w154_regs entry);

#ifdef __cplusplus
}
#endif

#endif
