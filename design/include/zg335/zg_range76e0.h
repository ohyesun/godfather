#ifndef ZG_RANGE76E0_H
#define ZG_RANGE76E0_H

#include <stdint.h>
#if !defined(UINT32_MAX) || UINT32_MAX != 0xffffffffu
#error "range76e0 requires exact uint32_t"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ZG_R76_COUNT_LIMIT32 UINT32_C(0x071c71c6)
#define ZG_R76_LENGTH_MESSAGE32 UINT32_C(0x101e3c70)

typedef enum zg_r76_state {
    ZG_R76_RETURNED = 0,
    ZG_R76_NONRETURN = 1,
    ZG_R76_ERROR = 2
} zg_r76_state;

typedef enum zg_r76_reason {
    ZG_R76_REASON_NONE = 0,
    ZG_R76_REASON_BAD_INTERFACE = 1,
    ZG_R76_REASON_SERVICE_STOP = 2,
    ZG_R76_REASON_INVALID_STATE = 3,
    ZG_R76_REASON_ABI_UNCONFIRMED = 4,
    ZG_R76_REASON_LENGTH_ERROR_RETURN_FRONTIER = 5
} zg_r76_reason;

typedef enum zg_r76_stage {
    ZG_R76_STAGE_INTERFACE = 0,
    ZG_R76_STAGE_READ_INITIAL_CURRENT = 1,
    ZG_R76_STAGE_READ_LAST = 2,
    ZG_R76_STAGE_READ_POSITION = 3,
    ZG_R76_STAGE_READ_POSITION_PREV = 4,
    ZG_R76_STAGE_CLONE = 5,
    ZG_R76_STAGE_READ_COUNT = 6,
    ZG_R76_STAGE_LENGTH_ERROR = 7,
    ZG_R76_STAGE_WRITE_COUNT = 8,
    ZG_R76_STAGE_WRITE_POSITION_PREV = 9,
    ZG_R76_STAGE_READ_NODE_PREV = 10,
    ZG_R76_STAGE_WRITE_PREDECESSOR_NEXT = 11,
    ZG_R76_STAGE_RELOAD_CURRENT = 12,
    ZG_R76_STAGE_READ_CURRENT_NEXT = 13,
    ZG_R76_STAGE_WRITE_CURRENT = 14,
    ZG_R76_STAGE_DONE = 15
} zg_r76_stage;

/* Actual EAX word on a RETURNED clone. Exactly 1 must attest the normal
 * continuation premise: ordinary return, cleanup of three pushed words,
 * preserved q/position/control-frame roles and accessible argument cells.
 * This does not forbid word-memory/argument-cell mutations, and does not
 * prove a native ABI or the opaque 0x100075e0 algorithm.
 */
typedef struct zg_r76_clone_result {
    zg_r76_state state;
    uint32_t eax32;
    uint32_t normal_abi_preserved;
} zg_r76_clone_result;

/* Fresh logical little-endian word access. Addresses are uint32 IDs, including
 * zero, unaligned and wrapped IDs if the service supports them. No ID is a
 * host pointer. RETURNED read must initialize *word_out; the host output cell
 * is trusted live storage. A stop may already have word-memory effects.
 */
typedef zg_r76_state (*zg_r76_read_fn)(
    void *context, uint32_t address32, uint32_t *word_out);
typedef zg_r76_state (*zg_r76_write_fn)(
    void *context, uint32_t address32, uint32_t word32);
typedef zg_r76_clone_result (*zg_r76_clone_fn)(
    void *context, uint32_t q32, uint32_t position32,
    uint32_t observed_previous32, uint32_t payload_address32);

/* Receives original numeric message word 0x101e3c70, not a host char pointer.
 * NONRETURN records a terminal prefix. RETURNED is an unsupported continuation
 * frontier: this API reports ERROR and performs no later range operations.
 * It neither invents volatile EAX/EDX nor models fallthrough, rollback or free.
 */
typedef zg_r76_state (*zg_r76_length_error_fn)(
    void *context, uint32_t message_word32);

typedef struct zg_r76_services {
    void *context;
    zg_r76_read_fn read32;
    zg_r76_write_fn write32;
    zg_r76_clone_fn clone;
    zg_r76_length_error_fn length_error;
} zg_r76_services;

typedef struct zg_r76_result {
    zg_r76_state state;
    zg_r76_reason reason;
    /* The attempted operation at stop, or DONE after an equal top compare. */
    zg_r76_stage stage;
    /* Valid only for RETURNED/DONE: current EAX word equal to fresh last.
     * Otherwise zero is only an invalid-result placeholder, not machine EAX.
     */
    uint32_t return_value32;
} zg_r76_result;

/* Ordered normal loop of 0x100076e0 in a word32 service model.
 * argument_base32 names the position argument cell; +4 is current, +8 last,
 * +12 is ignored. All address/count arithmetic wraps modulo 2^32.
 *
 * New host policy copies context/all hooks and validates all four hooks before
 * any effect, even for an empty range. Valid trusted callbacks return declared
 * states, initialize successful read outputs, preserve this C call's objects
 * and control flow, and can mutate word memory through legitimate service
 * effects. They must not retain/write the C output-cell pointers afterward.
 * The copied service table must initially be a live initialized C object.
 * NONRETURN is a logical stopped prefix, not a C _Noreturn assertion.
 * No retries, cleanup, arbitrary iteration budget or termination guarantee is
 * added. Inputs/services must make each operation return and eventually reach
 * equality or an explicit stop if a terminating host call is required.
 */
zg_r76_result zg_range76e0_normal(
    const zg_r76_services *services,
    uint32_t q32,
    uint32_t argument_base32);

#ifdef __cplusplus
}
#endif

#endif
