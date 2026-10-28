#ifndef ZG_COPY335_RAW_NORMAL_FRAME_V1_H
#define ZG_COPY335_RAW_NORMAL_FRAME_V1_H

/* Design declarations only. No implementation or native ABI is supplied.
 * Normative semantics: CONTRACT.json and INTERFACE.md in this package.
 * Struct padding is never an observable word or a serialization format.
 */
#include <limits.h>
#include <stdint.h>

#if CHAR_BIT != 8
#error ZG335_requires_8_bit_bytes
#endif
#if UINT32_MAX != 0xffffffffU
#error ZG335_requires_exact_uint32_t
#endif

#define ZG335_VERSION UINT32_C(1)
#define ZG335_SPACE_RAM UINT32_C(0)
#define ZG335_SPACE_LOGICAL_FS UINT32_C(1)

/* Zero is deliberately neither OK nor RETURNED. */
#define ZG335_SERVICE_UNSET UINT32_C(0)
#define ZG335_SERVICE_OK UINT32_C(1)
#define ZG335_SERVICE_STOPPED UINT32_C(2)
#define ZG335_SERVICE_INVALID UINT32_C(3)
#define ZG335_SERVICE_UNKNOWN UINT32_C(4)
#define ZG335_LOWER_UNSET UINT32_C(0)
#define ZG335_LOWER_RETURNED UINT32_C(1)
#define ZG335_LOWER_STOPPED UINT32_C(2)
#define ZG335_LOWER_INVALID UINT32_C(3)
#define ZG335_LOWER_UNKNOWN UINT32_C(4)

#define ZG335_RESULT_UNSET UINT32_C(0)
#define ZG335_BAD_INTERFACE UINT32_C(1)
#define ZG335_BAD_ENTRY UINT32_C(2)
#define ZG335_MEMORY_STOPPED UINT32_C(3)
#define ZG335_MEMORY_INVALID UINT32_C(4)
#define ZG335_MEMORY_UNKNOWN UINT32_C(5)
#define ZG335_MEMORY_REPLY_INVALID UINT32_C(6)
#define ZG335_CALLEE_STOPPED UINT32_C(7)
#define ZG335_CALLEE_INVALID UINT32_C(8)
#define ZG335_CALLEE_UNKNOWN UINT32_C(9)
#define ZG335_CALLEE_REPLY_INVALID UINT32_C(10)
#define ZG335_RETURN_CONTROL_INVALID UINT32_C(11)
#define ZG335_RETURNED UINT32_C(12)

#define ZG335_REASON_NONE UINT32_C(0)
#define ZG335_REASON_INTERFACE_VERSION UINT32_C(1)
#define ZG335_REASON_REQUIRED_HOOK UINT32_C(2)
#define ZG335_REASON_ENTRY_PRESENCE UINT32_C(3)
#define ZG335_REASON_SERVICE_STATE UINT32_C(4)
#define ZG335_REASON_READ_INITIALIZATION UINT32_C(5)
#define ZG335_REASON_WRITE_PREFIX UINT32_C(6)
#define ZG335_REASON_LOWER_STATE UINT32_C(7)
#define ZG335_REASON_RETURNED_PRESENCE UINT32_C(8)
#define ZG335_REASON_RETURN_TARGET UINT32_C(9)
#define ZG335_REASON_CLEANUP_RANGE UINT32_C(10)
#define ZG335_REASON_RETURN_ESP UINT32_C(11)

#define ZG335_EVENT_UNSET UINT32_C(0)
#define ZG335_EVENT_READ UINT32_C(1)
#define ZG335_EVENT_WRITE UINT32_C(2)

typedef struct zg335_regs8 {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
} zg335_regs8;

/* w0 is bits31:0, w1 bits63:32, w2 bits95:64, w3 bits127:96. */
typedef struct zg335_xmm0 {
    uint32_t w0, w1, w2, w3;
} zg335_xmm0;

typedef struct zg335_block16 {
    uint8_t byte[16];
} zg335_block16;

typedef struct zg335_entry {
    uint32_t regs_present;        /* exactly1 for an admitted entry */
    zg335_regs8 regs;             /* all eight words initialized */
    uint32_t xmm0_present;        /* exactly1, including early-stop input */
    zg335_xmm0 xmm0;              /* all four words initialized */
} zg335_entry;

typedef struct zg335_read_reply {
    uint32_t state32;
    uint32_t reason32;            /* opaque service diagnostic word */
    uint32_t initialized_mask32;  /* bit i for byte i; no bits >=width */
    zg335_block16 data;           /* unused bytes are initialized zero */
} zg335_read_reply;

typedef struct zg335_write_reply {
    uint32_t state32;
    uint32_t reason32;
    uint32_t committed_prefix32;  /* exact logical prefix0..width */
} zg335_write_reply;

/* There is no instruction-PC, ordinal, observer or host pointer guest address
 * parameter on memory hooks. Width is exactly1,2,4,8 or16. */
typedef void (*zg335_read_fn)(void *user, uint32_t space32,
    uint32_t address32, uint32_t width32, zg335_read_reply *reply);
typedef void (*zg335_write_fn)(void *user, uint32_t space32,
    uint32_t address32, uint32_t width32, zg335_block16 data,
    zg335_write_reply *reply);

typedef struct zg335_memory {
    void *user;
    zg335_read_fn read;
    zg335_write_fn write;
} zg335_memory;

typedef struct zg335_call {
    uint32_t ordinal32;           /* static call1..8; model origin only */
    uint32_t call_pc32;
    uint32_t target32;
    uint32_t continuation32;
    zg335_regs8 entry;            /* actual state AFTER CALL's push */
    zg335_xmm0 xmm0_entry;
} zg335_call;

typedef struct zg335_lower_reply {
    uint32_t state32;
    uint32_t reason32;
    uint32_t regs_present;
    zg335_regs8 returned;
    uint32_t xmm0_present;
    zg335_xmm0 xmm0_returned;
    uint32_t returned_to_present;
    uint32_t returned_to32;
    uint32_t cleanup_present;
    uint32_t ret_cleanup32;
} zg335_lower_reply;

/* request is by value. memory points to this invocation's private snapshot.
 * The lower service must itself freshly read arguments and the return cell;
 * target/continuation are not permission to synthesize those reads/results.
 */
typedef void (*zg335_lower_fn)(void *user, zg335_call request,
    const zg335_memory *memory, zg335_lower_reply *reply);

typedef struct zg335_services {
    uint32_t version32;
    zg335_memory memory;
    void *e4c0_user;
    zg335_lower_fn e4c0;
    void *fe0_user;
    zg335_lower_fn fe0;
} zg335_services;

typedef struct zg335_memory_event {
    uint32_t kind32;
    uint32_t space32;
    uint32_t address32;
    uint32_t width32;
    zg335_block16 write_data;
    zg335_read_reply read_reply;
    zg335_write_reply write_reply;
} zg335_memory_event;

typedef struct zg335_result {
    uint32_t state32;
    uint32_t reason32;
    uint32_t origin_pc_present;
    uint32_t origin_pc32;         /* diagnostic original-byte origin */
    uint32_t last_completed_pc_present;
    uint32_t last_completed_pc32;
    uint32_t entry_esp_present;
    uint32_t entry_esp32;         /* S, diagnostic only; never a frame base */
    uint32_t regs_present;
    zg335_regs8 regs;
    uint32_t xmm0_present;
    zg335_xmm0 xmm0;
    uint32_t calls_dispatched32;
    uint32_t calls_continued32;
    uint32_t last_memory_present;
    zg335_memory_event last_memory;
    uint32_t last_call_present;
    zg335_call last_call;
    uint32_t last_lower_reply_present;
    zg335_lower_reply last_lower_reply;
    uint32_t final_target_present;
    uint32_t final_target32;       /* fresh RET4 read, never followed natively */
} zg335_result;

/* Future portable implementation declaration only; no body is supplied. */
zg335_result zg335_raw_normal_frame(zg335_entry entry,
    const zg335_services *services);

#endif
