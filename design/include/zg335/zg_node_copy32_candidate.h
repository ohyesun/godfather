#ifndef ZG_NODE_COPY32_CANDIDATE_H
#define ZG_NODE_COPY32_CANDIDATE_H

#include <stdint.h>

#if UINT32_MAX != 0xffffffffu
#error "This bounded candidate requires an exact uint32_t type."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* SOURCE431 prospective primitive only. No registered API/module or acceptance. */
typedef enum zg_nc32_state {
    ZG_NC32_RETURNED = 0,
    ZG_NC32_NONRETURN = 1,
    ZG_NC32_ERROR = 2
} zg_nc32_state;

typedef enum zg_nc32_reason {
    ZG_NC32_REASON_NONE = 0,
    ZG_NC32_REASON_BAD_INTERFACE = 1,
    ZG_NC32_REASON_SERVICE_STOP = 2,
    ZG_NC32_REASON_INVALID_STATE = 3,
    ZG_NC32_REASON_ABI_UNCONFIRMED = 4
} zg_nc32_reason;

typedef enum zg_nc32_stage {
    ZG_NC32_STAGE_INTERFACE = 0,
    ZG_NC32_STAGE_READ_SOURCE0 = 1,
    ZG_NC32_STAGE_WRITE_DEST0 = 2,
    ZG_NC32_STAGE_READ_SOURCE4 = 3,
    ZG_NC32_STAGE_COMPARE_DEST4 = 4,
    ZG_NC32_STAGE_INC_INCOMING_WEAK = 5,
    ZG_NC32_STAGE_READ_OLD_DEST4 = 6,
    ZG_NC32_STAGE_DEC_OLD_WEAK = 7,
    ZG_NC32_STAGE_READ_FIRST_VTABLE = 8,
    ZG_NC32_STAGE_READ_FIRST_SLOT8 = 9,
    ZG_NC32_STAGE_CALL_FIRST_SLOT8 = 10,
    ZG_NC32_STAGE_WRITE_DEST4 = 11,
    ZG_NC32_STAGE_READ_SOURCE20 = 12,
    ZG_NC32_STAGE_WRITE_DEST20 = 13,
    ZG_NC32_STAGE_READ_SOURCE16 = 14,
    ZG_NC32_STAGE_WRITE_DEST16 = 15,
    ZG_NC32_STAGE_READ_SOURCE24 = 16,
    ZG_NC32_STAGE_WRITE_DEST24 = 17,
    ZG_NC32_STAGE_SELF_FIXUP16 = 18,
    ZG_NC32_STAGE_READ_SOURCE12 = 19,
    ZG_NC32_STAGE_READ_SOURCE8 = 20,
    ZG_NC32_STAGE_INC_INCOMING_STRONG = 21,
    ZG_NC32_STAGE_WRITE_DEST8 = 22,
    ZG_NC32_STAGE_READ_OLD_DEST12 = 23,
    ZG_NC32_STAGE_WRITE_DEST12 = 24,
    ZG_NC32_STAGE_DEC_OLD_STRONG = 25,
    ZG_NC32_STAGE_READ_SECOND_VTABLE4 = 26,
    ZG_NC32_STAGE_READ_SECOND_SLOT4 = 27,
    ZG_NC32_STAGE_CALL_SECOND_SLOT4 = 28,
    ZG_NC32_STAGE_DEC_WEAK_AFTER_SLOT4 = 29,
    ZG_NC32_STAGE_READ_SECOND_VTABLE8 = 30,
    ZG_NC32_STAGE_READ_SECOND_SLOT8 = 31,
    ZG_NC32_STAGE_CALL_SECOND_SLOT8 = 32,
    ZG_NC32_STAGE_DONE = 33
} zg_nc32_stage;

/* RETURNED requires a valid output word. Address words are never host pointers.
 * atomic_xadd32 performs ONE atomic32 RMW and reports its pre-update word.
 * A stop/error may already have effects; the primitive never retries/rolls back.
 */
typedef zg_nc32_state (*zg_nc32_read_fn)(
    void *context, uint32_t address32, uint32_t *word_out);
typedef zg_nc32_state (*zg_nc32_write_fn)(
    void *context, uint32_t address32, uint32_t word);
typedef zg_nc32_state (*zg_nc32_xadd_fn)(
    void *context, uint32_t address32, uint32_t delta32,
    uint32_t *old_word_out);

typedef struct zg_nc32_opaque_result {
    zg_nc32_state state;
    /* Exactly1 certifies the bounded normal-return ABI premise; not observed
     * original behavior. EBX/ESI/EDI/EBP live tokens and ESP must be preserved.
     */
    uint32_t normal_abi_preserved;
} zg_nc32_opaque_result;

typedef zg_nc32_opaque_result (*zg_nc32_slot_fn)(
    void *context, uint32_t object_this32,
    uint32_t freshly_read_vtable32, uint32_t freshly_read_target32);

typedef struct zg_nc32_services {
    void *context;
    zg_nc32_read_fn read32;
    zg_nc32_write_fn write32;
    zg_nc32_xadd_fn atomic_xadd32;
    zg_nc32_slot_fn call_slot4;
    zg_nc32_slot_fn call_slot8;
} zg_nc32_services;

typedef struct zg_nc32_result {
    zg_nc32_state state;
    zg_nc32_reason reason;
    zg_nc32_stage stage;
    uint32_t destination32;
    /* Valid only when state==RETURNED and stage==DONE. */
    uint32_t return_value32;
} zg_nc32_result;

/* Prospective source contract for the two identical175B bodies only.
 * All5 hooks are validated and copied before the first service event. Hook
 * table/context changes during calls do not rebind this invocation. Reads of
 * object/vtable MEMORY remain fresh. This is explicit portable policy.
 * NONRETURN reports logical cessation to the caller; it is not C _Noreturn.
 */
zg_nc32_result zg_nc32_copy7_candidate(
    const zg_nc32_services *services,
    uint32_t destination32, uint32_t source32);

#ifdef __cplusplus
}
#endif

#endif
