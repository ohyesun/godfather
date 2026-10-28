#include "zg_range101e0_raw_frame.h"

typedef struct zg_re4_raw_frame {
    zg_re4_raw_services ops;
    zg_w154_regs r;
    zg_re4_raw_result result;
} zg_re4_raw_frame;

static uint32_t zg_re4_raw_add(uint32_t a, uint32_t b)
{
    return (uint32_t)((unsigned long long)a + (unsigned long long)b);
}

static uint32_t zg_re4_raw_sub(uint32_t a, uint32_t b)
{
    return (uint32_t)((unsigned long long)a + 0x100000000ULL -
        (unsigned long long)b);
}

static void zg_re4_raw_attempt(
    zg_re4_raw_frame *f, uint32_t pc, zg_re4_raw_phase phase)
{
    f->result.instruction_pc_present = UINT32_C(1);
    f->result.instruction_pc32 = pc;
    f->result.phase = phase;
    f->result.reported_state_present = UINT32_C(0);
    f->result.reported_state = ZG_W154_UNKNOWN;
}

static int zg_re4_raw_checked(zg_re4_raw_frame *f, zg_w154_state state)
{
    f->result.reported_state = state;
    f->result.reported_state_present = UINT32_C(1);
    if (state == ZG_W154_RETURNED) {
        return 1;
    }
    if (state == ZG_W154_NONRETURN || state == ZG_W154_ERROR ||
        state == ZG_W154_UNKNOWN) {
        f->result.state = state;
        f->result.reason = ZG_RE4_RAW_REASON_SERVICE_STOP;
    } else {
        f->result.state = ZG_W154_ERROR;
        f->result.reason = ZG_RE4_RAW_REASON_INVALID_STATE;
    }
    return 0;
}

static int zg_re4_raw_read(
    zg_re4_raw_frame *f, uint32_t pc, zg_re4_raw_phase phase,
    uint32_t address, uint32_t *out)
{
    uint32_t word = UINT32_C(0);
    zg_w154_state state;
    zg_re4_raw_attempt(f, pc, phase);
    state = f->ops.wrapper.read32(f->ops.wrapper.context, address, &word);
    if (!zg_re4_raw_checked(f, state)) {
        return 0;
    }
    *out = word;
    return 1;
}

static int zg_re4_raw_write(
    zg_re4_raw_frame *f, uint32_t pc, zg_re4_raw_phase phase,
    uint32_t address, uint32_t value)
{
    zg_re4_raw_attempt(f, pc, phase);
    return zg_re4_raw_checked(f,
        f->ops.wrapper.write32(f->ops.wrapper.context, address, value));
}

static int zg_re4_raw_push(
    zg_re4_raw_frame *f, uint32_t pc, zg_re4_raw_phase phase, uint32_t value)
{
    f->r.esp32 = zg_re4_raw_add(f->r.esp32, UINT32_C(0xfffffffc));
    return zg_re4_raw_write(f, pc, phase, f->r.esp32, value);
}

static int zg_re4_raw_pop(
    zg_re4_raw_frame *f, uint32_t pc, uint32_t *out)
{
    uint32_t word = UINT32_C(0);
    if (!zg_re4_raw_read(f, pc, ZG_RE4_RAW_PHASE_POP_READ,
            f->r.esp32, &word)) {
        return 0;
    }
    f->r.esp32 = zg_re4_raw_add(f->r.esp32, UINT32_C(4));
    *out = word;
    return 1;
}

static int zg_re4_raw_fs_read(zg_re4_raw_frame *f, uint32_t pc)
{
    uint32_t word = UINT32_C(0);
    zg_w154_state state;
    zg_re4_raw_attempt(f, pc, ZG_RE4_RAW_PHASE_READ_FS_HEAD);
    state = f->ops.wrapper.read_fs_head32(f->ops.wrapper.context, &word);
    if (!zg_re4_raw_checked(f, state)) {
        return 0;
    }
    f->r.eax32 = word;
    return 1;
}

static int zg_re4_raw_fs_write(
    zg_re4_raw_frame *f, uint32_t pc, uint32_t value)
{
    zg_re4_raw_attempt(f, pc, ZG_RE4_RAW_PHASE_WRITE_FS_HEAD);
    return zg_re4_raw_checked(f,
        f->ops.wrapper.write_fs_head32(f->ops.wrapper.context, value));
}

static int zg_re4_raw_frontier(
    zg_re4_raw_frame *f, zg_re4_raw_reason reason)
{
    f->result.state = ZG_W154_UNKNOWN;
    f->result.reason = reason;
    return 0;
}

static int zg_re4_raw_clone(zg_re4_raw_frame *f)
{
    zg_w154_result reply;
    uint32_t expected_esp = zg_re4_raw_add(f->r.esp32, UINT32_C(16));
    zg_re4_raw_attempt(f, UINT32_C(0x10010225),
        ZG_RE4_RAW_PHASE_CD40_WRAPPER);
    reply = zg_wrapper154_candidate(&f->ops.wrapper, ZG_W154_CD40, f->r);
    f->result.wrapper_result = reply;
    f->result.wrapper_result_present = UINT32_C(1);
    f->r = reply.registers32;
    /* Preflight and fixed CD40 make every present reply non-INTERFACE. */
    f->result.nested_pc_present = UINT32_C(1);
    f->result.nested_pc32 = zg_re4_raw_add(
        ZG_RE4_RAW_CD40_TARGET32, reply.instruction_offset32);
    if (!zg_re4_raw_checked(f, reply.state)) {
        return 0;
    }
    if (reply.reason != ZG_W154_REASON_NONE ||
        reply.phase != ZG_W154_PHASE_DONE) {
        return zg_re4_raw_frontier(f, ZG_RE4_RAW_REASON_CD40_CONTROL_FRONTIER);
    }
    if (reply.wrapper_ret_cleanup32 != UINT32_C(12) ||
        reply.return_target32 != ZG_RE4_RAW_CD40_CONTINUATION32 ||
        f->r.esp32 != expected_esp) {
        return zg_re4_raw_frontier(f, ZG_RE4_RAW_REASON_CD40_CONTROL_FRONTIER);
    }
    return 1;
}

static int zg_re4_raw_length(zg_re4_raw_frame *f)
{
    uint32_t target = UINT32_C(0);
    zg_re4_raw_length_request request;
    zg_w154_state state;
    if (!zg_re4_raw_read(f, ZG_RE4_RAW_LENGTH_CALL_PC32,
            ZG_RE4_RAW_PHASE_LENGTH_IAT_READ, ZG_RE4_RAW_LENGTH_IAT32,
            &target)) {
        return 0;
    }
    f->result.length_target32 = target;
    f->result.length_target_present = UINT32_C(1);
    if (!zg_re4_raw_push(f, ZG_RE4_RAW_LENGTH_CALL_PC32,
            ZG_RE4_RAW_PHASE_LENGTH_RETURN_PC_STORE,
            ZG_RE4_RAW_LENGTH_CONTINUATION32)) {
        return 0;
    }
    request.target32 = target;
    request.callPC32 = ZG_RE4_RAW_LENGTH_CALL_PC32;
    request.continuation32 = ZG_RE4_RAW_LENGTH_CONTINUATION32;
    request.encoded_message_literal32 = ZG_RE4_RAW_MESSAGE_LITERAL32;
    request.entry = f->r;
    f->result.length_request = request;
    f->result.length_request_present = UINT32_C(1);
    zg_re4_raw_attempt(f, ZG_RE4_RAW_LENGTH_CALL_PC32,
        ZG_RE4_RAW_PHASE_LENGTH_DISPATCH);
    state = f->ops.length_call(f->ops.length_context, request);
    if (!zg_re4_raw_checked(f, state)) {
        return 0;
    }
    return zg_re4_raw_frontier(f, ZG_RE4_RAW_REASON_LENGTH_RETURN_FRONTIER);
}

#define ZG_RE4_RAW_TRY(expression) \
    do { \
        if (!(expression)) { \
            goto stopped; \
        } \
    } while (0)

zg_re4_raw_result zg_range101e0_raw_frame_normal(
    const zg_re4_raw_services *services, zg_w154_regs entry)
{
    zg_re4_raw_frame f = {0};
    uint32_t word = UINT32_C(0);
    uint32_t return_target = UINT32_C(0);
    int no_carry;

    f.r = entry;
    f.result.state = ZG_W154_ERROR;
    f.result.reason = ZG_RE4_RAW_REASON_BAD_INTERFACE;
    f.result.phase = ZG_RE4_RAW_PHASE_INTERFACE;
    f.result.registers32 = entry;
    f.result.reported_state = ZG_W154_UNKNOWN;
    f.result.wrapper_result.state = ZG_W154_ERROR;
    f.result.wrapper_result.reason = ZG_W154_REASON_BAD_INTERFACE;
    f.result.wrapper_result.phase = ZG_W154_PHASE_INTERFACE;
    if (services == 0) {
        return f.result;
    }
    f.ops = *services;
    if (f.ops.wrapper.read32 == 0 || f.ops.wrapper.write32 == 0 ||
        f.ops.wrapper.write8 == 0 || f.ops.wrapper.read_fs_head32 == 0 ||
        f.ops.wrapper.write_fs_head32 == 0 || f.ops.wrapper.call_first == 0 ||
        f.ops.wrapper.call_second == 0 || f.ops.length_call == 0) {
        return f.result;
    }

    /* site 01 0x100101e0 */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x100101e0),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, f.r.ebp32));
    /* site 02 0x100101e1 */
    f.r.ebp32 = f.r.esp32;
    /* site 03 0x100101e3 */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x100101e3),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, UINT32_C(0xffffffff)));
    /* site 04 0x100101e5 */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x100101e5),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, ZG_RE4_RAW_HANDLER_LITERAL32));
    /* site 05 0x100101ea */
    ZG_RE4_RAW_TRY(zg_re4_raw_fs_read(&f, UINT32_C(0x100101ea)));
    /* site 06 0x100101f0 */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x100101f0),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, f.r.eax32));
    /* site 07 0x100101f1 */
    ZG_RE4_RAW_TRY(zg_re4_raw_fs_write(&f, UINT32_C(0x100101f1), f.r.esp32));
    /* site 08 0x100101f8 */
    f.r.esp32 = zg_re4_raw_add(f.r.esp32, UINT32_C(0xfffffff4));
    /* site 09 0x100101fb */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x100101fb),
        ZG_RE4_RAW_PHASE_READ32, zg_re4_raw_add(f.r.ebp32, UINT32_C(12)),
        &f.r.eax32));
    /* site 10 0x100101fe */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x100101fe),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, f.r.ebx32));
    /* site 11 0x100101ff */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x100101ff),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, f.r.esi32));
    /* site 12 0x10010200 */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x10010200),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, f.r.edi32));
    /* site 13 0x10010201 */
    f.r.edi32 = f.r.ecx32;
    /* site 14 0x10010203 */
    ZG_RE4_RAW_TRY(zg_re4_raw_write(&f, UINT32_C(0x10010203),
        ZG_RE4_RAW_PHASE_WRITE32, zg_re4_raw_add(f.r.ebp32, UINT32_C(0xfffffff0)),
        f.r.esp32));
    /* site 15 0x10010206 */
    ZG_RE4_RAW_TRY(zg_re4_raw_write(&f, UINT32_C(0x10010206),
        ZG_RE4_RAW_PHASE_WRITE32, zg_re4_raw_add(f.r.ebp32, UINT32_C(0xffffffe8)),
        f.r.edi32));
    /* site 16 0x10010209 */
    ZG_RE4_RAW_TRY(zg_re4_raw_write(&f, UINT32_C(0x10010209),
        ZG_RE4_RAW_PHASE_WRITE32, zg_re4_raw_add(f.r.ebp32, UINT32_C(0xffffffec)),
        f.r.eax32));
    /* site 17 0x1001020c */
    ZG_RE4_RAW_TRY(zg_re4_raw_write(&f, UINT32_C(0x1001020c),
        ZG_RE4_RAW_PHASE_WRITE32, zg_re4_raw_add(f.r.ebp32, UINT32_C(0xfffffffc)),
        UINT32_C(0)));

loop_top:
    /* site 18 0x10010213 */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x10010213),
        ZG_RE4_RAW_PHASE_READ32, zg_re4_raw_add(f.r.ebp32, UINT32_C(16)), &word));
    /* site 19 0x10010216 */
    if (f.r.eax32 == word) {
        goto epilogue;
    }
    /* site 20 0x10010218 */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x10010218),
        ZG_RE4_RAW_PHASE_READ32, zg_re4_raw_add(f.r.ebp32, UINT32_C(8)),
        &f.r.esi32));
    /* site 21 0x1001021b */
    f.r.eax32 = zg_re4_raw_add(f.r.eax32, UINT32_C(8));
    /* site 22 0x1001021e */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x1001021e),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, f.r.eax32));
    /* site 23 0x1001021f */
    f.r.ecx32 = f.r.edi32;
    /* site 24 0x10010221 */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x10010221),
        ZG_RE4_RAW_PHASE_PUSH_OPERAND_READ,
        zg_re4_raw_add(f.r.esi32, UINT32_C(4)), &word));
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x10010221),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, word));
    /* site 25 0x10010224 */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x10010224),
        ZG_RE4_RAW_PHASE_PUSH_WRITE, f.r.esi32));
    /* site 26 0x10010225 */
    ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x10010225),
        ZG_RE4_RAW_PHASE_CD40_RETURN_PC_STORE, ZG_RE4_RAW_CD40_CONTINUATION32));
    ZG_RE4_RAW_TRY(zg_re4_raw_clone(&f));
    /* site 27 0x1001022a */
    f.r.edx32 = f.r.eax32;
    /* site 28 0x1001022c */
    f.r.ecx32 = ZG_RE4_RAW_COUNT_LIMIT32;
    /* site 29 0x10010231 */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x10010231),
        ZG_RE4_RAW_PHASE_READ32, zg_re4_raw_add(f.r.edi32, UINT32_C(4)),
        &f.r.eax32));
    /* site 30 0x10010234 */
    f.r.ecx32 = zg_re4_raw_sub(f.r.ecx32, f.r.eax32);
    /* site 31 0x10010236 */
    no_carry = f.r.ecx32 >= UINT32_C(1);
    /* site 32 0x10010239 */
    if (!no_carry) {
        /* site 33 0x1001023b */
        ZG_RE4_RAW_TRY(zg_re4_raw_push(&f, UINT32_C(0x1001023b),
            ZG_RE4_RAW_PHASE_PUSH_WRITE, ZG_RE4_RAW_MESSAGE_LITERAL32));
        /* site 34 0x10010240 */
        ZG_RE4_RAW_TRY(zg_re4_raw_length(&f));
    }
    /* site 35 0x10010246 */
    f.r.eax32 = zg_re4_raw_add(f.r.eax32, UINT32_C(1));
    /* site 36 0x10010247 */
    ZG_RE4_RAW_TRY(zg_re4_raw_write(&f, UINT32_C(0x10010247),
        ZG_RE4_RAW_PHASE_WRITE32, zg_re4_raw_add(f.r.edi32, UINT32_C(4)),
        f.r.eax32));
    /* site 37 0x1001024a */
    ZG_RE4_RAW_TRY(zg_re4_raw_write(&f, UINT32_C(0x1001024a),
        ZG_RE4_RAW_PHASE_WRITE32, zg_re4_raw_add(f.r.esi32, UINT32_C(4)),
        f.r.edx32));
    /* site 38 0x1001024d */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x1001024d),
        ZG_RE4_RAW_PHASE_READ32, zg_re4_raw_add(f.r.edx32, UINT32_C(4)),
        &f.r.eax32));
    /* site 39 0x10010250 */
    ZG_RE4_RAW_TRY(zg_re4_raw_write(&f, UINT32_C(0x10010250),
        ZG_RE4_RAW_PHASE_WRITE32, f.r.eax32, f.r.edx32));
    /* site 40 0x10010252 */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x10010252),
        ZG_RE4_RAW_PHASE_READ32, zg_re4_raw_add(f.r.ebp32, UINT32_C(12)),
        &f.r.eax32));
    /* site 41 0x10010255 */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x10010255),
        ZG_RE4_RAW_PHASE_READ32, f.r.eax32, &f.r.eax32));
    /* site 42 0x10010257 */
    ZG_RE4_RAW_TRY(zg_re4_raw_write(&f, UINT32_C(0x10010257),
        ZG_RE4_RAW_PHASE_WRITE32, zg_re4_raw_add(f.r.ebp32, UINT32_C(12)),
        f.r.eax32));
    /* site 43 0x1001025a */
    goto loop_top;

epilogue:
    /* site 44 0x10010284 */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x10010284),
        ZG_RE4_RAW_PHASE_READ32, zg_re4_raw_add(f.r.ebp32, UINT32_C(0xfffffff4)),
        &f.r.ecx32));
    /* site 45 0x10010287 */
    ZG_RE4_RAW_TRY(zg_re4_raw_pop(&f, UINT32_C(0x10010287), &f.r.edi32));
    /* site 46 0x10010288 */
    ZG_RE4_RAW_TRY(zg_re4_raw_pop(&f, UINT32_C(0x10010288), &f.r.esi32));
    /* site 47 0x10010289 */
    ZG_RE4_RAW_TRY(zg_re4_raw_fs_write(&f, UINT32_C(0x10010289), f.r.ecx32));
    /* site 48 0x10010290 */
    ZG_RE4_RAW_TRY(zg_re4_raw_pop(&f, UINT32_C(0x10010290), &f.r.ebx32));
    /* site 49 0x10010291 */
    f.r.esp32 = f.r.ebp32;
    /* site 50 0x10010293 */
    ZG_RE4_RAW_TRY(zg_re4_raw_pop(&f, UINT32_C(0x10010293), &f.r.ebp32));
    /* site 51 0x10010294 */
    ZG_RE4_RAW_TRY(zg_re4_raw_read(&f, UINT32_C(0x10010294),
        ZG_RE4_RAW_PHASE_RET_TARGET_READ, f.r.esp32, &return_target));
    f.r.esp32 = zg_re4_raw_add(f.r.esp32, UINT32_C(20));
    f.result.state = ZG_W154_RETURNED;
    f.result.reason = ZG_RE4_RAW_REASON_NONE;
    f.result.phase = ZG_RE4_RAW_PHASE_DONE;
    f.result.final_return_valid = UINT32_C(1);
    f.result.return_value32 = f.r.eax32;
    f.result.return_target32 = return_target;
    f.result.return_cleanup32 = UINT32_C(16);

stopped:
    f.result.registers32 = f.r;
    return f.result;
}

#undef ZG_RE4_RAW_TRY
