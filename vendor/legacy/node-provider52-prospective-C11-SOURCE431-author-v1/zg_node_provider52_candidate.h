#ifndef ZG_NODE_PROVIDER52_CANDIDATE_H
#define ZG_NODE_PROVIDER52_CANDIDATE_H
#include <stdint.h>
#if UINT32_MAX != 0xffffffffu
#error "This candidate requires exact uint32_t."
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Prospective bounded SOURCE431 provider only; no registered API/acceptance. */
typedef enum zg_np52_state {
    ZG_NP52_RETURNED = 0, ZG_NP52_NONRETURN = 1, ZG_NP52_ERROR = 2
} zg_np52_state;
typedef enum zg_np52_reason {
    ZG_NP52_REASON_NONE = 0, ZG_NP52_REASON_BAD_INTERFACE = 1,
    ZG_NP52_REASON_SERVICE_STOP = 2, ZG_NP52_REASON_INVALID_STATE = 3,
    ZG_NP52_REASON_ABI_UNCONFIRMED = 4
} zg_np52_reason;
typedef enum zg_np52_stage {
    ZG_NP52_STAGE_INTERFACE = 0, ZG_NP52_STAGE_ALLOCATE36 = 1,
    ZG_NP52_STAGE_FAILURE_IMPORT = 2, ZG_NP52_STAGE_READ_ARG_A = 3,
    ZG_NP52_STAGE_READ_ARG_B = 4, ZG_NP52_STAGE_WRITE_FIRST = 5,
    ZG_NP52_STAGE_WRITE_SECOND = 6, ZG_NP52_STAGE_DONE = 7
} zg_np52_stage;
/* On RETURNED, eax32 is the ACTUAL post-call EAX word. This includes a
 * returning failure import; do not fabricate zero or retain pre-call EAX.
 * normal_abi_preserved must be exactly1: an explicit adapter premise that
 * caller continuation/stack arguments remain accessible with normal cleanup.
 * This flag does not prove native ABI or CRT behavior.
 */
typedef struct zg_np52_call_result {
    zg_np52_state state;
    uint32_t eax32;
    uint32_t normal_abi_preserved;
} zg_np52_call_result;
typedef zg_np52_call_result (*zg_np52_allocate_fn)(void *context, uint32_t bytes32);
/* No original ordinary argument is invented for this no-argument import. */
typedef zg_np52_call_result (*zg_np52_failure_fn)(void *context);
/* Symbolic incoming argument slots1/2, not native stack-pointer arithmetic.
 * They are read AFTER calls. B(index2) is never read when A(index1)==0.
 * RETURNED requires a valid output. Caller supplies trusted live storage.
 */
typedef zg_np52_state (*zg_np52_argument_fn)(
    void *context, uint32_t argument_index, uint32_t *word_out);
typedef zg_np52_state (*zg_np52_write_fn)(
    void *context, uint32_t address32, uint32_t word);
typedef struct zg_np52_services {
    void *context;
    zg_np52_allocate_fn allocate;
    zg_np52_failure_fn failure_import;
    zg_np52_argument_fn read_argument32;
    zg_np52_write_fn write32;
} zg_np52_services;
typedef struct zg_np52_result {
    zg_np52_state state;
    zg_np52_reason reason;
    zg_np52_stage stage;
    /* Valid only when state==RETURNED and stage==DONE. */
    uint32_t return_value32;
} zg_np52_result;
/* All four hooks/context are copied and validated before any service effect.
 * This is new portable policy. Memory capabilities and output storage must
 * remain valid for trusted normal callbacks. No host pointer is made from IDs.
 * Logical NONRETURN reports a prefix, not C _Noreturn. A stopping service may
 * already have effects; there is no retry, cleanup, rollback or free here.
 * No allocation ownership, concurrency, CRT/SEH or native lifetime equivalence.
 */
zg_np52_result zg_np52_provider_candidate(const zg_np52_services *services);
#ifdef __cplusplus
}
#endif
#endif
