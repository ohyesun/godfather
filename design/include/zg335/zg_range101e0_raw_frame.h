#ifndef ZG_RANGE101E0_RAW_FRAME_PROPOSAL_H
#define ZG_RANGE101E0_RAW_FRAME_PROPOSAL_H

#include <stdint.h>
#include "zg_wrapper154_candidate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Proposed raw-frame normal CONTRACT only; no C product/adapter/oracle here. */
#define ZG_RE4_RAW_ENTRY32 UINT32_C(0x100101e0)
#define ZG_RE4_RAW_CD40_TARGET32 UINT32_C(0x1000cd40)
#define ZG_RE4_RAW_CD40_CONTINUATION32 UINT32_C(0x1001022a)
#define ZG_RE4_RAW_COUNT_LIMIT32 UINT32_C(0x071c71c6)
#define ZG_RE4_RAW_LENGTH_CALL_PC32 UINT32_C(0x10010240)
#define ZG_RE4_RAW_LENGTH_IAT32 UINT32_C(0x101d7270)
#define ZG_RE4_RAW_LENGTH_CONTINUATION32 UINT32_C(0x10010246)
#define ZG_RE4_RAW_MESSAGE_LITERAL32 UINT32_C(0x101e3c70)
#define ZG_RE4_RAW_HANDLER_LITERAL32 UINT32_C(0x101be970)

typedef enum zg_re4_raw_reason {
    ZG_RE4_RAW_REASON_NONE = 0,
    ZG_RE4_RAW_REASON_BAD_INTERFACE = 1,
    ZG_RE4_RAW_REASON_SERVICE_STOP = 2,
    ZG_RE4_RAW_REASON_INVALID_STATE = 3,
    ZG_RE4_RAW_REASON_CD40_CONTROL_FRONTIER = 4,
    ZG_RE4_RAW_REASON_LENGTH_RETURN_FRONTIER = 5
} zg_re4_raw_reason;

typedef enum zg_re4_raw_phase {
    ZG_RE4_RAW_PHASE_INTERFACE = 0,
    ZG_RE4_RAW_PHASE_READ32 = 1,
    ZG_RE4_RAW_PHASE_WRITE32 = 2,
    ZG_RE4_RAW_PHASE_READ_FS_HEAD = 3,
    ZG_RE4_RAW_PHASE_WRITE_FS_HEAD = 4,
    ZG_RE4_RAW_PHASE_PUSH_OPERAND_READ = 5,
    ZG_RE4_RAW_PHASE_PUSH_WRITE = 6,
    ZG_RE4_RAW_PHASE_CD40_RETURN_PC_STORE = 7,
    ZG_RE4_RAW_PHASE_CD40_WRAPPER = 8,
    ZG_RE4_RAW_PHASE_LENGTH_IAT_READ = 9,
    ZG_RE4_RAW_PHASE_LENGTH_RETURN_PC_STORE = 10,
    ZG_RE4_RAW_PHASE_LENGTH_DISPATCH = 11,
    ZG_RE4_RAW_PHASE_POP_READ = 12,
    ZG_RE4_RAW_PHASE_RET_TARGET_READ = 13,
    ZG_RE4_RAW_PHASE_DONE = 14
} zg_re4_raw_phase;

/* Captured destination and entry AFTER the successful indirect CALL PC store.
 * callPC=0x10010240, continuation=0x10010246. The encoded literal is metadata,
 * NOT a fresh callee argument read or proof that its guest cell still contains
 * that value. The opaque callee owns its own argument/return-slot/effect reads.
 * No host target call, known callee body, cleanup or returned register set.
 */
typedef struct zg_re4_raw_length_request {
    uint32_t target32;
    uint32_t callPC32;
    uint32_t continuation32;
    uint32_t encoded_message_literal32;
    zg_w154_regs entry;
} zg_re4_raw_length_request;

typedef zg_w154_state (*zg_re4_raw_length_fn)(
    void *context, zg_re4_raw_length_request request);

/* Snapshot the complete embedded seven-hook/context table and separate length
 * hook/context once before any callback; validate all EIGHT hooks even when
 * initial current will equal last. NULL contexts are allowed. No hook gains a
 * PC parameter. No observer/history/acknowledgment capability is introduced.
 * All words/bytes/stack/arguments/object/IAT and any aliased logical FS-head
 * share coherent little-endian wrapping byte RAM. Ordinary memory/FS hooks
 * preserve logical registers. Callee hooks alone supply new register records.
 */
typedef struct zg_re4_raw_services {
    zg_w154_services wrapper;
    void *length_context;
    zg_re4_raw_length_fn length_call;
} zg_re4_raw_services;

/* Every field is initialized. Presence/validity fields are numeric exact0/1.
 * state uses inherited RETURNED/NONRETURN/ERROR/UNKNOWN distinctly.
 * registers32 is always the last DEFINED logical snapshot (entry on interface
 * rejection). It does not infer opaque callee endpoint registers. At a nested
 * wrapper stop use its documented last-defined registers32; at a length stop
 * retain the request entry snapshot, since length_call returns no registers.
 */
typedef struct zg_re4_raw_result {
    zg_w154_state state;
    zg_re4_raw_reason reason;
    zg_re4_raw_phase phase;
    uint32_t instruction_pc_present;
    uint32_t instruction_pc32;
    zg_w154_regs registers32;
    /* The state actually returned by the current attempted service/wrapper,
     * including invalid representable enum values. Ignored if presence0. */
    uint32_t reported_state_present;
    zg_w154_state reported_state;
    /* Latest-only entire initialized nested result; copying the whole C struct
     * and its documented last-defined registers is valid on every C reply,
     * including stopped states. Classify its actual state FIRST. Only RETURNED
     * enters reason NONE/phase DONE validation; only after both pass may final
     * return words be semantically consumed. No per-iteration history. */
    uint32_t wrapper_result_present;
    zg_w154_result wrapper_result;
    /* Derived preferred CD40 PC=0x1000cd40+offset only for a present wrapper
     * non-INTERFACE phase from the fixed library; its phase is in the result.
     * This is independent of the current outer instruction_pc32. */
    uint32_t nested_pc_present;
    uint32_t nested_pc32;
    /* A successful IAT operand read captures target even if the later PC store
     * stops. The full request is present only once dispatch is actually made. */
    uint32_t length_target_present;
    uint32_t length_target32;
    uint32_t length_request_present;
    zg_re4_raw_length_request length_request;
    /* Only RETURNED/NONE/DONE has final_return_valid1 and valid EAX/target/16.
     * All other outcomes use final_return_valid0 and all three final words0.
     * A guest return target is reported as data, never executed or cast. */
    uint32_t final_return_valid;
    uint32_t return_value32;
    uint32_t return_target32;
    uint32_t return_cleanup32;
} zg_re4_raw_result;

/* Caller supplies all eight initialized logical registers, entry ESP=S and
 * ECX=q, with already mapped/initialized return cell[S] and four incoming cells
 * [S+4,+8,+12,+16]. A=S+4 is conditional address algebra, never a frame producer
 * or descriptor-derived S. No entry seeding/probe before original PUSH EBP.
 * All arithmetic wraps32; zero/unaligned/partial/wrapped aliases are allowed
 * when mapped. Guest effects cannot corrupt live C control/snapshot/storage.
 * No protected-prefix filter/noninterference flag or separate guest stacks.
 *
 * Emit all selected51 outer sites in CONTRACT order, including prologue FS,
 * saved/local stores and real payload PUSH BEFORE the fresh position.prev read.
 * Register/immediate PUSH: capture value, decrement ESP4, write. Memory PUSH:
 * read operand first; only RETURNED permits decrement/write. A write stop keeps
 * the decrement and committed hook effects. POP: read oldESP; only RETURNED
 * increments ESP4 then assigns the destination. Reserved bytes are not zeroed.
 * PC tags belong to the static operation table and stop diagnostics only.
 *
 * Direct CD40 CALL first decrements ESP4/writes0x1001022a, then invokes unchanged
 * wrapper154 CD40 exactly once. First copy its whole initialized rich result,
 * documented last-defined registers and actual reported state. Whole C struct
 * copying remains valid for stopped states; semantic classification is ordered:
 * NONRETURN/ERROR/UNKNOWN stops in the SAME raw state with SERVICE_STOP; any
 * other nonRETURNED representable state stops ERROR/INVALID_STATE. These paths
 * do not semantically inspect reason/phase/cleanup/target/final return words.
 * ONLY actual RETURNED validates reason NONE and phase DONE; failure stops
 * UNKNOWN/CD40_CONTROL_FRONTIER before consuming final return words. Only after
 * those checks require cleanup12, target0x1001022a and actual returned ESP equal
 * that CD40-entryESP+16wrap; mismatch stops UNKNOWN/CD40_CONTROL_FRONTIER. This
 * frontier applies ONLY within the RETURNED domain. No stop has later effects.
 * After full success use ALL actual returned registers. Do NOT compare returned
 * EBP/ESI/EDI/EBX to entry values: legal guest saved-slot aliases may change them.
 * No abstract E4 kernel/clone adapter replays prefix.
 *
 * The length CALL uses the explicitly conditional SLEIGH flat32 sequence:
 * completed message PUSH; fresh IAT read[0x101d7270] captures destination; ESP-4;
 * write0x10010246; dispatch by-value request to typed length_call. A stopping IAT
 * read cannot decrement/store/dispatch; a stopping PC write cannot dispatch.
 * Later PC-store aliasing cannot alter captured destination. No guest target is
 * a host pointer. NONRETURN/ERROR/UNKNOWN stops distinctly; RETURNED becomes
 * UNKNOWN/LENGTH_RETURN_FRONTIER with no inferred volatile endpoint registers,
 * INC/store/fallthrough/gap/SEH. Invalid hook states become ERROR/INVALID_STATE.
 *
 * Successful epilogue reads actual current EBP-relative saved FS head and all
 * fresh POP/RET cells. RET target read must return before ESP+=4+16 and final
 * data reporting; do not require target/nonvolatiles equal entry snapshots.
 * Read outputs start0 and are ignored unless RETURNED. Stopping callbacks may
 * have committed effects. No later callback, cleanup, rollback/free, observer,
 * retry, acknowledgment, iteration cap or termination guarantee. Logical
 * NONRETURN is diagnostic returned data, not C _Noreturn.
 *
 * Selected 32-bit little-endian flat profile only: noIA64/LONGMODE_OFF/vexMode0,
 * addrsize1/opsize1, no FS/GS base addition for the absolute IAT operand. Explicit
 * logical FS operations remain. Conditional decoder SOURCE order is not low
 * pcode/compiledSLA/CPU/fault/segment/realIAT/native ABI/SEH/OS evidence. 143B/51
 * sites only; excluded40B and handler body remain unmodeled. No lower-callee
 * binding or full335/handoff caller-frame producer is provided.
 */
zg_re4_raw_result zg_range101e0_raw_frame_normal(
    const zg_re4_raw_services *services, zg_w154_regs entry);

#ifdef __cplusplus
}
#endif
#endif
