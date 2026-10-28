#include "zg_wrapper154_candidate.h"

typedef struct zg_w154_frame {
    zg_w154_services ops;
    zg_w154_regs r;
    zg_w154_reason reason;
    uint32_t offset;
    zg_w154_phase phase;
} zg_w154_frame;

static uint32_t zg_w154_add(uint32_t a, uint32_t b)
{
    /* ULL is at least64bits, including hosts with unusual integer promotion. */
    return (uint32_t)((unsigned long long)a + (unsigned long long)b);
}

static zg_w154_state zg_w154_checked(zg_w154_frame *f, zg_w154_state state)
{
    if (state == ZG_W154_RETURNED) {
        return state;
    }
    if (state == ZG_W154_NONRETURN || state == ZG_W154_ERROR ||
        state == ZG_W154_UNKNOWN) {
        f->reason = ZG_W154_REASON_SERVICE_STOP;
        return state;
    }
    f->reason = ZG_W154_REASON_INVALID_STATE;
    return ZG_W154_ERROR;
}

static zg_w154_state zg_w154_read(
    zg_w154_frame *f, uint32_t offset, uint32_t address, uint32_t *out)
{
    uint32_t word;
    zg_w154_state state;
    f->offset = offset;
    f->phase = ZG_W154_PHASE_READ32;
    state = zg_w154_checked(f, f->ops.read32(f->ops.context, address, &word));
    if (state == ZG_W154_RETURNED) {
        *out = word;
    }
    return state;
}

static zg_w154_state zg_w154_write(
    zg_w154_frame *f, uint32_t offset, uint32_t address, uint32_t value)
{
    f->offset = offset;
    f->phase = ZG_W154_PHASE_WRITE32;
    return zg_w154_checked(f, f->ops.write32(f->ops.context, address, value));
}

static zg_w154_state zg_w154_byte(
    zg_w154_frame *f, uint32_t offset, uint32_t address, uint32_t value)
{
    f->offset = offset;
    f->phase = ZG_W154_PHASE_WRITE8;
    return zg_w154_checked(f, f->ops.write8(f->ops.context, address, value));
}

static zg_w154_state zg_w154_push(
    zg_w154_frame *f, uint32_t offset, uint32_t value)
{
    f->r.esp32 = zg_w154_add(f->r.esp32, UINT32_C(0xfffffffc));
    return zg_w154_write(f, offset, f->r.esp32, value);
}

static zg_w154_state zg_w154_pop(
    zg_w154_frame *f, uint32_t offset, uint32_t *out)
{
    zg_w154_state state = zg_w154_read(f, offset, f->r.esp32, out);
    if (state == ZG_W154_RETURNED) {
        f->r.esp32 = zg_w154_add(f->r.esp32, UINT32_C(4));
    }
    return state;
}

static zg_w154_state zg_w154_fs_read(zg_w154_frame *f, uint32_t *out)
{
    uint32_t word;
    zg_w154_state state;
    f->offset = UINT32_C(10);
    f->phase = ZG_W154_PHASE_READ_FS_HEAD;
    state = zg_w154_checked(f, f->ops.read_fs_head32(f->ops.context, &word));
    if (state == ZG_W154_RETURNED) {
        *out = word;
    }
    return state;
}

static zg_w154_state zg_w154_fs_write(
    zg_w154_frame *f, uint32_t offset, uint32_t value)
{
    f->offset = offset;
    f->phase = ZG_W154_PHASE_WRITE_FS_HEAD;
    return zg_w154_checked(f, f->ops.write_fs_head32(f->ops.context, value));
}

static zg_w154_state zg_w154_call_boundary(
    zg_w154_frame *f, uint32_t offset, uint32_t ordinal,
    uint32_t target, uint32_t continuation, uint32_t cleanup,
    zg_w154_callee_fn invoke)
{
    zg_w154_call call;
    zg_w154_call_result reply;
    zg_w154_state state;
    uint32_t expected_esp;

    /* A near CALL writes its continuation AFTER all preceding object stores.
     * This write shares memory with arguments, object and frame bytes.
     */
    state = zg_w154_push(f, offset, continuation);
    if (state != ZG_W154_RETURNED) {
        return state;
    }
    call.ordinal32 = ordinal;
    call.target32 = target;
    call.continuation32 = continuation;
    call.callee_ret_cleanup32 = cleanup;
    call.entry = f->r;
    expected_esp = zg_w154_add(call.entry.esp32,
        zg_w154_add(UINT32_C(4), cleanup));
    f->offset = offset;
    f->phase = ZG_W154_PHASE_CALLEE;
    reply = invoke(f->ops.context, call);
    state = zg_w154_checked(f, reply.state);
    if (state != ZG_W154_RETURNED) {
        return state;
    }
    f->r = reply.returned;
    if (reply.normal_abi_preserved != UINT32_C(1)) {
        f->reason = ZG_W154_REASON_ABI_UNCONFIRMED;
        return ZG_W154_UNKNOWN;
    }
    if (reply.returned_to32 != continuation ||
        f->r.esp32 != expected_esp ||
        f->r.ebp32 != call.entry.ebp32 ||
        f->r.ebx32 != call.entry.ebx32 ||
        f->r.esi32 != call.entry.esi32 ||
        f->r.edi32 != call.entry.edi32) {
        f->reason = ZG_W154_REASON_ABI_MISMATCH;
        return ZG_W154_UNKNOWN;
    }
    return ZG_W154_RETURNED;
}

#define ZG_W154_TRY(expression) \
    do { \
        state = (expression); \
        if (state != ZG_W154_RETURNED) { \
            goto stopped; \
        } \
    } while (0)

zg_w154_result zg_wrapper154_candidate(
    const zg_w154_services *services,
    zg_w154_variant variant, zg_w154_regs entry)
{
    zg_w154_frame f;
    zg_w154_result result;
    zg_w154_state state;
    uint32_t handler;
    uint32_t first_target;
    uint32_t second_target;
    uint32_t first_continuation;
    uint32_t second_continuation;
    uint32_t word;
    uint32_t return_target;

    result.state = ZG_W154_ERROR;
    result.reason = ZG_W154_REASON_BAD_INTERFACE;
    result.instruction_offset32 = UINT32_C(0);
    result.phase = ZG_W154_PHASE_INTERFACE;
    result.registers32 = entry;
    result.return_value32 = UINT32_C(0);
    result.return_target32 = UINT32_C(0);
    result.wrapper_ret_cleanup32 = UINT32_C(0);
    if (services == 0) {
        return result;
    }
    f.ops = *services;
    if (f.ops.read32 == 0 || f.ops.write32 == 0 || f.ops.write8 == 0 ||
        f.ops.read_fs_head32 == 0 || f.ops.write_fs_head32 == 0 ||
        f.ops.call_first == 0 || f.ops.call_second == 0) {
        return result;
    }
    if (variant == ZG_W154_CD40) {
        handler = UINT32_C(0x101be854);
        first_target = UINT32_C(0x1002ee30);
        second_target = UINT32_C(0x10024470);
        first_continuation = UINT32_C(0x1000cd6c);
        second_continuation = UINT32_C(0x1000cdc5);
    } else if (variant == ZG_W154_75E0) {
        handler = UINT32_C(0x101be1c4);
        first_target = UINT32_C(0x1000a130);
        second_target = UINT32_C(0x10008610);
        first_continuation = UINT32_C(0x1000760c);
        second_continuation = UINT32_C(0x10007665);
    } else {
        return result;
    }
    f.r = entry;
    f.reason = ZG_W154_REASON_NONE;
    f.offset = UINT32_C(0);
    f.phase = ZG_W154_PHASE_INTERFACE;

    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(0), f.r.ebp32));
    f.r.ebp32 = f.r.esp32;                                      /* +1 */
    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(3), UINT32_C(0xffffffff)));
    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(5), handler));
    ZG_W154_TRY(zg_w154_fs_read(&f, &f.r.eax32));
    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(16), f.r.eax32));
    ZG_W154_TRY(zg_w154_fs_write(&f, UINT32_C(17), f.r.esp32));
    f.r.esp32 = zg_w154_add(f.r.esp32, UINT32_C(0xfffffff8));      /* +24 */
    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(27), f.r.ebx32));
    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(28), f.r.esi32));
    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(29), f.r.edi32));
    ZG_W154_TRY(zg_w154_write(&f, UINT32_C(30),
        zg_w154_add(f.r.ebp32, UINT32_C(0xfffffff0)), f.r.esp32));
    ZG_W154_TRY(zg_w154_read(&f, UINT32_C(33),
        zg_w154_add(f.r.ebp32, UINT32_C(12)), &word));
    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(33), word));
    ZG_W154_TRY(zg_w154_read(&f, UINT32_C(36),
        zg_w154_add(f.r.ebp32, UINT32_C(8)), &word));
    ZG_W154_TRY(zg_w154_push(&f, UINT32_C(36), word));
    ZG_W154_TRY(zg_w154_call_boundary(&f, UINT32_C(39), UINT32_C(1),
        first_target, first_continuation, UINT32_C(8), f.ops.call_first));

    f.r.esi32 = f.r.eax32;                                      /* +44 N */
    ZG_W154_TRY(zg_w154_write(&f, UINT32_C(46),
        zg_w154_add(f.r.ebp32, UINT32_C(0xfffffffc)), UINT32_C(0)));
    ZG_W154_TRY(zg_w154_write(&f, UINT32_C(53),
        zg_w154_add(f.r.ebp32, UINT32_C(12)), f.r.esi32));
    f.r.ecx32 = zg_w154_add(f.r.esi32, UINT32_C(8));              /* +56 */
    ZG_W154_TRY(zg_w154_write(&f, UINT32_C(59),
        zg_w154_add(f.r.ebp32, UINT32_C(8)), f.r.ecx32));
    ZG_W154_TRY(zg_w154_write(&f, UINT32_C(62),
        zg_w154_add(f.r.ebp32, UINT32_C(0xffffffec)), f.r.ecx32));
    ZG_W154_TRY(zg_w154_byte(&f, UINT32_C(65),
        zg_w154_add(f.r.ebp32, UINT32_C(0xfffffffc)), UINT32_C(1)));

    /* +69/+71 test the wrapped N+8 word, including N=0 and N=fffffff8. */
    if (f.r.ecx32 != UINT32_C(0)) {
        ZG_W154_TRY(zg_w154_write(&f, UINT32_C(73), f.r.ecx32, UINT32_C(0)));
        ZG_W154_TRY(zg_w154_write(&f, UINT32_C(79),
            zg_w154_add(f.r.ecx32, UINT32_C(4)), UINT32_C(0)));
        ZG_W154_TRY(zg_w154_write(&f, UINT32_C(86),
            zg_w154_add(f.r.ecx32, UINT32_C(8)), UINT32_C(0)));
        ZG_W154_TRY(zg_w154_write(&f, UINT32_C(93),
            zg_w154_add(f.r.ecx32, UINT32_C(12)), UINT32_C(0)));
        ZG_W154_TRY(zg_w154_read(&f, UINT32_C(100),
            zg_w154_add(f.r.ebp32, UINT32_C(16)), &word));
        ZG_W154_TRY(zg_w154_push(&f, UINT32_C(100), word));
        ZG_W154_TRY(zg_w154_byte(&f, UINT32_C(103),
            zg_w154_add(f.r.ebp32, UINT32_C(0xfffffffc)), UINT32_C(3)));
        ZG_W154_TRY(zg_w154_write(&f, UINT32_C(107),
            zg_w154_add(f.r.ecx32, UINT32_C(16)), UINT32_C(0)));
        ZG_W154_TRY(zg_w154_write(&f, UINT32_C(114),
            zg_w154_add(f.r.ecx32, UINT32_C(20)), UINT32_C(0)));
        ZG_W154_TRY(zg_w154_write(&f, UINT32_C(121),
            zg_w154_add(f.r.ecx32, UINT32_C(24)), UINT32_C(0)));
        ZG_W154_TRY(zg_w154_call_boundary(&f, UINT32_C(128), UINT32_C(2),
            second_target, second_continuation, UINT32_C(4), f.ops.call_second));
    }

    /* Fresh saved-head load AFTER second call; second EAX is not used. */
    ZG_W154_TRY(zg_w154_read(&f, UINT32_C(133),
        zg_w154_add(f.r.ebp32, UINT32_C(0xfffffff4)), &f.r.ecx32));
    f.r.eax32 = f.r.esi32;                                      /* +136 */
    ZG_W154_TRY(zg_w154_pop(&f, UINT32_C(138), &f.r.edi32));
    ZG_W154_TRY(zg_w154_pop(&f, UINT32_C(139), &f.r.esi32));
    ZG_W154_TRY(zg_w154_fs_write(&f, UINT32_C(140), f.r.ecx32));
    ZG_W154_TRY(zg_w154_pop(&f, UINT32_C(147), &f.r.ebx32));
    f.r.esp32 = f.r.ebp32;                                      /* +148 */
    ZG_W154_TRY(zg_w154_pop(&f, UINT32_C(150), &f.r.ebp32));
    ZG_W154_TRY(zg_w154_pop(&f, UINT32_C(151), &return_target));
    f.r.esp32 = zg_w154_add(f.r.esp32, UINT32_C(12));              /* RET12 */
    result.state = ZG_W154_RETURNED;
    result.reason = ZG_W154_REASON_NONE;
    result.instruction_offset32 = UINT32_C(151);
    result.phase = ZG_W154_PHASE_DONE;
    result.registers32 = f.r;
    result.return_value32 = f.r.eax32;
    result.return_target32 = return_target;
    result.wrapper_ret_cleanup32 = UINT32_C(12);
    return result;

stopped:
    result.state = state;
    result.reason = f.reason;
    result.instruction_offset32 = f.offset;
    result.phase = f.phase;
    result.registers32 = f.r;
    return result;
}

#undef ZG_W154_TRY
