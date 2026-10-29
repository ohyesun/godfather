#ifndef ZG_NORMAL116_FULLSTATE_DRAFT_V1_H
#define ZG_NORMAL116_FULLSTATE_DRAFT_V1_H
/* Independent contract draft only: declarations, no C implementation.
 * Logical fields only; padding/host pointer bits are never serialized state.
 * CONTRACT.json and INTERFACE.md define the selected normal-frame domain.
 */
#include <limits.h>
#include <stdint.h>
#if CHAR_BIT != 8
#error ZG116_requires_8_bit_bytes
#endif
#if UINT32_MAX != 0xffffffffU
#error ZG116_requires_exact_uint32_t
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define ZG116_VERSION UINT32_C(1)
#define ZG116_ENTRY_7FE0 UINT32_C(0x10007fe0)
#define ZG116_ENTRY_E4C0 UINT32_C(0x1001e4c0)
#define ZG116_SPACE_RAM UINT32_C(0)
#define ZG116_SPACE_LOGICAL_FS UINT32_C(1)
#define ZG116_SERVICE_UNSET UINT32_C(0)
#define ZG116_SERVICE_OK UINT32_C(1)
#define ZG116_SERVICE_STOPPED UINT32_C(2)
#define ZG116_SERVICE_INVALID UINT32_C(3)
#define ZG116_SERVICE_UNKNOWN UINT32_C(4)
#define ZG116_LOWER_UNSET UINT32_C(0)
#define ZG116_LOWER_RETURNED UINT32_C(1)
#define ZG116_LOWER_STOPPED UINT32_C(2)
#define ZG116_LOWER_INVALID UINT32_C(3)
#define ZG116_LOWER_UNKNOWN UINT32_C(4)
#define ZG116_ROLE_SENTINEL UINT32_C(1)
#define ZG116_ROLE_RANGE UINT32_C(2)
#define ZG116_EVENT_UNSET UINT32_C(0)
#define ZG116_EVENT_READ UINT32_C(1)
#define ZG116_EVENT_WRITE UINT32_C(2)

#define ZG116_RESULT_UNSET UINT32_C(0)
#define ZG116_BAD_INTERFACE UINT32_C(1)
#define ZG116_BAD_ENTRY UINT32_C(2)
#define ZG116_BAD_WRAPPER UINT32_C(3)
#define ZG116_MEMORY_STOPPED UINT32_C(4)
#define ZG116_MEMORY_INVALID UINT32_C(5)
#define ZG116_MEMORY_UNKNOWN UINT32_C(6)
#define ZG116_MEMORY_REPLY_INVALID UINT32_C(7)
#define ZG116_CALLEE_STOPPED UINT32_C(8)
#define ZG116_CALLEE_INVALID UINT32_C(9)
#define ZG116_CALLEE_UNKNOWN UINT32_C(10)
#define ZG116_CALLEE_REPLY_INVALID UINT32_C(11)
#define ZG116_RETURN_CONTROL_INVALID UINT32_C(12)
#define ZG116_RETURNED UINT32_C(13)
#define ZG116_REASON_NONE UINT32_C(0)
#define ZG116_REASON_INTERFACE_VERSION UINT32_C(1)
#define ZG116_REASON_REQUIRED_HOOK UINT32_C(2)
#define ZG116_REASON_ENTRY_PRESENCE UINT32_C(3)
#define ZG116_REASON_WRAPPER_ENTRY UINT32_C(4)
#define ZG116_REASON_SERVICE_STATE UINT32_C(5)
#define ZG116_REASON_READ_INITIALIZATION UINT32_C(6)
#define ZG116_REASON_WRITE_PREFIX UINT32_C(7)
#define ZG116_REASON_LOWER_STATE UINT32_C(8)
#define ZG116_REASON_RETURNED_PRESENCE UINT32_C(9)
#define ZG116_REASON_RETURN_TARGET UINT32_C(10)
#define ZG116_REASON_CLEANUP_RANGE UINT32_C(11)
#define ZG116_REASON_RETURN_ESP UINT32_C(12)

typedef struct zg116_regs8 {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
} zg116_regs8;
typedef struct zg116_xmm0 {
    uint32_t w0, w1, w2, w3;
} zg116_xmm0;
typedef struct zg116_block16 {
    uint8_t byte[16];
} zg116_block16;
typedef struct zg116_entry {
    uint32_t wrapper_entry32;     /* exactly one of the two fixed entries */
    uint32_t regs_present;        /* exactly1; all eight initialized words */
    zg116_regs8 regs;
    uint32_t xmm0_present;        /* exactly1; no inner preservation inferred */
    zg116_xmm0 xmm0;
} zg116_entry;
typedef struct zg116_read_reply {
    uint32_t state32;
    uint32_t reason32;            /* opaque provider word */
    uint32_t initialized_mask32;  /* low width bits; remaining bits0 on OK */
    zg116_block16 data;           /* unused bytes0 on OK */
} zg116_read_reply;
typedef struct zg116_write_reply {
    uint32_t state32;
    uint32_t reason32;
    uint32_t committed_prefix32;  /* actual increasing prefix0..width */
} zg116_write_reply;
typedef void (*zg116_read_fn)(void *user, uint32_t space32,
    uint32_t address32, uint32_t width32, zg116_read_reply *reply);
typedef void (*zg116_write_fn)(void *user, uint32_t space32,
    uint32_t address32, uint32_t width32, zg116_block16 data,
    zg116_write_reply *reply);
typedef struct zg116_memory {
    void *user;
    zg116_read_fn read;
    zg116_write_fn write;
} zg116_memory;

/* New inner-call domain. ordinal1..2 is not raw335's ordinal1..8.
 * Actual entry is AFTER the original CALL's continuation push.
 */
typedef struct zg116_call {
    uint32_t wrapper_entry32;
    uint32_t ordinal32;
    uint32_t role32;
    uint32_t call_pc32;
    uint32_t target32;
    uint32_t continuation32;
    zg116_regs8 entry;
    zg116_xmm0 xmm0_entry;
} zg116_call;
typedef struct zg116_lower_reply {
    uint32_t state32;
    uint32_t reason32;
    uint32_t regs_present;
    zg116_regs8 returned;
    uint32_t xmm0_present;
    zg116_xmm0 xmm0_returned;
    uint32_t returned_to_present;
    uint32_t returned_to32;
    uint32_t cleanup_present;
    uint32_t ret_cleanup32;
} zg116_lower_reply;
/* memory is the current invocation's private hook-table snapshot. No extra
 * PC/ordinal/observer is added to read/write. Lower owns fresh arg/RET reads.
 */
typedef void (*zg116_lower_fn)(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply);
typedef struct zg116_services {
    uint32_t version32;
    zg116_memory memory;
    void *sentinel_user;
    zg116_lower_fn sentinel;
    void *range_user;
    zg116_lower_fn range;
} zg116_services;
typedef struct zg116_memory_event {
    uint32_t kind32;
    uint32_t space32;
    uint32_t address32;
    uint32_t width32;
    zg116_block16 write_data;
    zg116_read_reply read_reply;
    zg116_write_reply write_reply;
} zg116_memory_event;
typedef struct zg116_result {
    uint32_t state32;
    uint32_t reason32;            /* consumer enum, not provider reason */
    uint32_t origin_pc_present;
    uint32_t origin_pc32;
    uint32_t last_completed_pc_present;
    uint32_t last_completed_pc32;
    uint32_t entry_esp_present;
    uint32_t entry_esp32;         /* diagnostic S, never immutable frame */
    uint32_t regs_present;
    zg116_regs8 regs;
    uint32_t xmm0_present;
    zg116_xmm0 xmm0;
    uint32_t calls_dispatched32;  /* actual inner dispatches,0..2 */
    uint32_t calls_continued32;   /* validated inner RETURNED,0..2 */
    uint32_t last_memory_present;
    zg116_memory_event last_memory; /* last OUTER event only, no history */
    uint32_t last_call_present;
    zg116_call last_call;
    uint32_t last_lower_reply_present;
    zg116_lower_reply last_lower_reply;
    uint32_t final_target_present;
    uint32_t final_target32;      /* fresh finalRET cell; never executed */
    uint32_t final_cleanup_present;
    uint32_t final_cleanup32;     /*4 only after selected RET4 completes */
} zg116_result;

/* Draft declaration only. Both original116-byte bodies remain isolated from
 * the old narrow APIs. No native function pointer is constructed from entry.
 */
zg116_result zg116_normal_frame(zg116_entry entry,
    const zg116_services *services);
#ifdef __cplusplus
}
#endif
#endif
