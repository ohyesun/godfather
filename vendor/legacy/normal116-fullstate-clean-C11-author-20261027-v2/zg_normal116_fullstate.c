/* Independent portable C11 implementation of the admitted normal116 profile.
 * Only the two selected linear 41-instruction streams are executed.
 * All guest words stay uint32_t; only typed host hooks perform memory effects.
 * The accepted header is copied without alteration. Padding is not state.
 * Instruction labels I01..I41 apply separately at each selected wrapper entry.
 */
#include "zg_normal116_fullstate.h"

static uint32_t zg116_add32(uint32_t a, uint32_t b)
{
    return (uint32_t)((uintmax_t)a + (uintmax_t)b);
}

static uint32_t zg116_sub32(uint32_t a, uint32_t b)
{
    return (uint32_t)((uintmax_t)a - (uintmax_t)b);
}

static int zg116_stop(zg116_result *r, uint32_t pc,
    uint32_t state, uint32_t reason)
{
    r->state32 = state;
    r->reason32 = reason;
    r->origin_pc_present = UINT32_C(1);
    r->origin_pc32 = pc;
    return 0;
}

static void zg116_completed(zg116_result *r, uint32_t pc)
{
    r->last_completed_pc_present = UINT32_C(1);
    r->last_completed_pc32 = pc;
}

/* One original read4 event. A failed read never changes its destination.
 * The event contains a raw initialized reply, even when it is malformed.
 */
static int zg116_read_word(zg116_result *r, const zg116_services *hooks,
    uint32_t pc, uint32_t space, uint32_t address, uint32_t *destination)
{
    zg116_read_reply reply = {0};
    zg116_memory_event event = {0};
    uint32_t i;
    event.kind32 = ZG116_EVENT_READ;
    event.space32 = space;
    event.address32 = address;
    event.width32 = UINT32_C(4);
    hooks->memory.read(hooks->memory.user, space, address, UINT32_C(4), &reply);
    event.read_reply = reply;
    r->last_memory = event;
    r->last_memory_present = UINT32_C(1);

    if (reply.state32 == ZG116_SERVICE_STOPPED) {
        return zg116_stop(r, pc, ZG116_MEMORY_STOPPED, ZG116_REASON_NONE);
    }
    if (reply.state32 == ZG116_SERVICE_INVALID) {
        return zg116_stop(r, pc, ZG116_MEMORY_INVALID, ZG116_REASON_NONE);
    }
    if (reply.state32 == ZG116_SERVICE_UNKNOWN) {
        return zg116_stop(r, pc, ZG116_MEMORY_UNKNOWN, ZG116_REASON_NONE);
    }
    if (reply.state32 != ZG116_SERVICE_OK) {
        return zg116_stop(r, pc, ZG116_MEMORY_REPLY_INVALID,
            ZG116_REASON_SERVICE_STATE);
    }
    if (reply.initialized_mask32 != UINT32_C(15)) {
        return zg116_stop(r, pc, ZG116_MEMORY_REPLY_INVALID,
            ZG116_REASON_READ_INITIALIZATION);
    }
    for (i = UINT32_C(4); i < UINT32_C(16); i = zg116_add32(i, UINT32_C(1))) {
        if (reply.data.byte[i] != 0) {
            return zg116_stop(r, pc, ZG116_MEMORY_REPLY_INVALID,
                ZG116_REASON_READ_INITIALIZATION);
        }
    }
    *destination = (uint32_t)((uint32_t)reply.data.byte[0]
        | ((uint32_t)reply.data.byte[1] << 8)
        | ((uint32_t)reply.data.byte[2] << 16)
        | ((uint32_t)reply.data.byte[3] << 24));
    return 1;
}

/* One original write of width1 or width4. The provider owns the committed
 * prefix; classification performs no rollback, retry or guest read.
 */
static int zg116_write_value(zg116_result *r, const zg116_services *hooks,
    uint32_t pc, uint32_t space, uint32_t address, uint32_t width, uint32_t value)
{
    zg116_block16 data = {0};
    zg116_write_reply reply = {0};
    zg116_memory_event event = {0};
    uint32_t i;
    for (i = UINT32_C(0); i < width; i = zg116_add32(i, UINT32_C(1))) {
        data.byte[i] = (uint8_t)((value >> (8 * i)) & UINT32_C(255));
    }
    event.kind32 = ZG116_EVENT_WRITE;
    event.space32 = space;
    event.address32 = address;
    event.width32 = width;
    event.write_data = data;
    hooks->memory.write(hooks->memory.user, space, address, width, data, &reply);
    event.write_reply = reply;
    r->last_memory = event;
    r->last_memory_present = UINT32_C(1);

    if (reply.state32 == ZG116_SERVICE_STOPPED) {
        return zg116_stop(r, pc, ZG116_MEMORY_STOPPED, ZG116_REASON_NONE);
    }
    if (reply.state32 == ZG116_SERVICE_INVALID) {
        return zg116_stop(r, pc, ZG116_MEMORY_INVALID, ZG116_REASON_NONE);
    }
    if (reply.state32 == ZG116_SERVICE_UNKNOWN) {
        return zg116_stop(r, pc, ZG116_MEMORY_UNKNOWN, ZG116_REASON_NONE);
    }
    if (reply.state32 != ZG116_SERVICE_OK) {
        return zg116_stop(r, pc, ZG116_MEMORY_REPLY_INVALID,
            ZG116_REASON_SERVICE_STATE);
    }
    if (reply.committed_prefix32 != width) {
        return zg116_stop(r, pc, ZG116_MEMORY_REPLY_INVALID,
            ZG116_REASON_WRITE_PREFIX);
    }
    return 1;
}

/* Passing value by value latches PUSH source before the ESP decrement. */
static int zg116_push_word(zg116_result *r, const zg116_services *hooks,
    uint32_t pc, uint32_t value)
{
    r->regs.esp = zg116_sub32(r->regs.esp, UINT32_C(4));
    return zg116_write_value(r, hooks, pc, ZG116_SPACE_RAM,
        r->regs.esp, UINT32_C(4), value);
}

static int zg116_pop_word(zg116_result *r, const zg116_services *hooks,
    uint32_t pc, uint32_t *destination)
{
    uint32_t value = UINT32_C(0);
    if (!zg116_read_word(r, hooks, pc, ZG116_SPACE_RAM, r->regs.esp, &value)) {
        return 0;
    }
    *destination = value;
    r->regs.esp = zg116_add32(r->regs.esp, UINT32_C(4));
    return 1;
}

/* CALL's return-cell write is an outer instruction event. The typed opaque
 * lower sees post-push state by value and the private memory hook snapshot.
 */
static int zg116_call_inner(zg116_result *r, const zg116_services *hooks,
    uint32_t wrapper, uint32_t pc, uint32_t ordinal, uint32_t role,
    uint32_t target, uint32_t continuation)
{
    zg116_call request = {0};
    zg116_lower_reply reply = {0};
    if (!zg116_push_word(r, hooks, pc, continuation)) {
        return 0;
    }
    request.wrapper_entry32 = wrapper;
    request.ordinal32 = ordinal;
    request.role32 = role;
    request.call_pc32 = pc;
    request.target32 = target;
    request.continuation32 = continuation;
    request.entry = r->regs;
    request.xmm0_entry = r->xmm0;
    r->last_call = request;
    r->last_call_present = UINT32_C(1);
    r->calls_dispatched32 = zg116_add32(r->calls_dispatched32, UINT32_C(1));
    if (role == ZG116_ROLE_SENTINEL) {
        hooks->sentinel(hooks->sentinel_user, request, &hooks->memory, &reply);
    } else {
        hooks->range(hooks->range_user, request, &hooks->memory, &reply);
    }
    r->last_lower_reply = reply;
    r->last_lower_reply_present = UINT32_C(1);

    /* Preserve the raw presence separately; publish only exactly1 images. */
    r->regs_present = UINT32_C(0);
    r->regs = (zg116_regs8){0};
    r->xmm0_present = UINT32_C(0);
    r->xmm0 = (zg116_xmm0){0};
    if (reply.regs_present == UINT32_C(1)) {
        r->regs_present = UINT32_C(1);
        r->regs = reply.returned;
    }
    if (reply.xmm0_present == UINT32_C(1)) {
        r->xmm0_present = UINT32_C(1);
        r->xmm0 = reply.xmm0_returned;
    }
    if (reply.state32 == ZG116_LOWER_STOPPED) {
        return zg116_stop(r, pc, ZG116_CALLEE_STOPPED, ZG116_REASON_NONE);
    }
    if (reply.state32 == ZG116_LOWER_INVALID) {
        return zg116_stop(r, pc, ZG116_CALLEE_INVALID, ZG116_REASON_NONE);
    }
    if (reply.state32 == ZG116_LOWER_UNKNOWN) {
        return zg116_stop(r, pc, ZG116_CALLEE_UNKNOWN, ZG116_REASON_NONE);
    }
    if (reply.state32 != ZG116_LOWER_RETURNED) {
        return zg116_stop(r, pc, ZG116_CALLEE_REPLY_INVALID,
            ZG116_REASON_LOWER_STATE);
    }
    if (reply.regs_present != UINT32_C(1)
        || reply.xmm0_present != UINT32_C(1)
        || reply.returned_to_present != UINT32_C(1)
        || reply.cleanup_present != UINT32_C(1)) {
        return zg116_stop(r, pc, ZG116_CALLEE_REPLY_INVALID,
            ZG116_REASON_RETURNED_PRESENCE);
    }
    if (reply.returned_to32 != continuation) {
        return zg116_stop(r, pc, ZG116_RETURN_CONTROL_INVALID,
            ZG116_REASON_RETURN_TARGET);
    }
    if (reply.ret_cleanup32 > UINT32_C(65535)) {
        return zg116_stop(r, pc, ZG116_RETURN_CONTROL_INVALID,
            ZG116_REASON_CLEANUP_RANGE);
    }
    if (reply.returned.esp != zg116_add32(request.entry.esp,
        zg116_add32(UINT32_C(4), reply.ret_cleanup32))) {
        return zg116_stop(r, pc, ZG116_RETURN_CONTROL_INVALID,
            ZG116_REASON_RETURN_ESP);
    }
    r->calls_continued32 = zg116_add32(r->calls_continued32, UINT32_C(1));
    return 1;
}

zg116_result zg116_normal_frame(zg116_entry entry,
    const zg116_services *services)
{
    zg116_result r = {0};
    zg116_services hooks;
    uint32_t wrapper;
    uint32_t handler;
    uint32_t sentinel_target;
    uint32_t range_target;
    uint32_t pc;
    uint32_t value = UINT32_C(0);

    /* P-ADMISSION: required hooks, then version, presence, wrapper. */
    if (services == 0) {
        r.state32 = ZG116_BAD_INTERFACE;
        r.reason32 = ZG116_REASON_REQUIRED_HOOK;
        return r;
    }
    hooks = *services;
    if (hooks.memory.read == 0 || hooks.memory.write == 0
        || hooks.sentinel == 0 || hooks.range == 0) {
        r.state32 = ZG116_BAD_INTERFACE;
        r.reason32 = ZG116_REASON_REQUIRED_HOOK;
        return r;
    }
    if (hooks.version32 != ZG116_VERSION) {
        r.state32 = ZG116_BAD_INTERFACE;
        r.reason32 = ZG116_REASON_INTERFACE_VERSION;
        return r;
    }
    if (entry.regs_present != UINT32_C(1) || entry.xmm0_present != UINT32_C(1)) {
        r.state32 = ZG116_BAD_ENTRY;
        r.reason32 = ZG116_REASON_ENTRY_PRESENCE;
        return r;
    }
    wrapper = entry.wrapper_entry32;
    if (wrapper == ZG116_ENTRY_7FE0) {
        handler = UINT32_C(0x101be328);
        sentinel_target = UINT32_C(0x1000a130);
        range_target = UINT32_C(0x100076e0);
    } else if (wrapper == ZG116_ENTRY_E4C0) {
        handler = UINT32_C(0x101bf9c8);
        sentinel_target = UINT32_C(0x1002ee30);
        range_target = UINT32_C(0x100101e0);
    } else {
        r.state32 = ZG116_BAD_WRAPPER;
        r.reason32 = ZG116_REASON_WRAPPER_ENTRY;
        return r;
    }
    r.entry_esp_present = UINT32_C(1);
    r.entry_esp32 = entry.regs.esp;
    r.regs_present = UINT32_C(1);
    r.regs = entry.regs;
    r.xmm0_present = UINT32_C(1);
    r.xmm0 = entry.xmm0;

    /* I01 +000 PUSH EBP */
    pc = zg116_add32(wrapper, UINT32_C(0));
    if (!zg116_push_word(&r, &hooks, pc, r.regs.ebp)) { return r; }
    zg116_completed(&r, pc);

    /* I02 +001 MOV EBP,ESP */
    pc = zg116_add32(wrapper, UINT32_C(1));
    r.regs.ebp = r.regs.esp;
    zg116_completed(&r, pc);

    /* I03 +003 PUSH -1 (6a ff sign extension) */
    pc = zg116_add32(wrapper, UINT32_C(3));
    if (!zg116_push_word(&r, &hooks, pc, UINT32_C(0xffffffff))) { return r; }
    zg116_completed(&r, pc);

    /* I04 +005 PUSH handler32 */
    pc = zg116_add32(wrapper, UINT32_C(5));
    if (!zg116_push_word(&r, &hooks, pc, handler)) { return r; }
    zg116_completed(&r, pc);

    /* I05 +010 MOV EAX,FS:[0] */
    pc = zg116_add32(wrapper, UINT32_C(10));
    if (!zg116_read_word(&r, &hooks, pc, ZG116_SPACE_LOGICAL_FS,
        UINT32_C(0), &r.regs.eax)) { return r; }
    zg116_completed(&r, pc);

    /* I06 +016 PUSH EAX */
    pc = zg116_add32(wrapper, UINT32_C(16));
    if (!zg116_push_word(&r, &hooks, pc, r.regs.eax)) { return r; }
    zg116_completed(&r, pc);

    /* I07 +017 MOV FS:[0],ESP */
    pc = zg116_add32(wrapper, UINT32_C(17));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_LOGICAL_FS,
        UINT32_C(0), UINT32_C(4), r.regs.esp)) { return r; }
    zg116_completed(&r, pc);

    /* I08 +024 SUB ESP,8 (flags excluded by profile) */
    pc = zg116_add32(wrapper, UINT32_C(24));
    r.regs.esp = zg116_sub32(r.regs.esp, UINT32_C(8));
    zg116_completed(&r, pc);

    /* I09 +027 PUSH EBX */
    pc = zg116_add32(wrapper, UINT32_C(27));
    if (!zg116_push_word(&r, &hooks, pc, r.regs.ebx)) { return r; }
    zg116_completed(&r, pc);

    /* I10 +028 PUSH ESI */
    pc = zg116_add32(wrapper, UINT32_C(28));
    if (!zg116_push_word(&r, &hooks, pc, r.regs.esi)) { return r; }
    zg116_completed(&r, pc);

    /* I11 +029 PUSH EDI */
    pc = zg116_add32(wrapper, UINT32_C(29));
    if (!zg116_push_word(&r, &hooks, pc, r.regs.edi)) { return r; }
    zg116_completed(&r, pc);

    /* I12 +030 MOV ESI,ECX */
    pc = zg116_add32(wrapper, UINT32_C(30));
    r.regs.esi = r.regs.ecx;
    zg116_completed(&r, pc);

    /* I13 +032 MOV [EBP-10h],ESP */
    pc = zg116_add32(wrapper, UINT32_C(32));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_RAM,
        zg116_sub32(r.regs.ebp, UINT32_C(16)), UINT32_C(4), r.regs.esp)) { return r; }
    zg116_completed(&r, pc);

    /* I14 +035 PUSH 0 */
    pc = zg116_add32(wrapper, UINT32_C(35));
    if (!zg116_push_word(&r, &hooks, pc, UINT32_C(0))) { return r; }
    zg116_completed(&r, pc);

    /* I15 +037 PUSH 0 */
    pc = zg116_add32(wrapper, UINT32_C(37));
    if (!zg116_push_word(&r, &hooks, pc, UINT32_C(0))) { return r; }
    zg116_completed(&r, pc);

    /* I16 +039 MOV [EBP-14h],ESI */
    pc = zg116_add32(wrapper, UINT32_C(39));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_RAM,
        zg116_sub32(r.regs.ebp, UINT32_C(20)), UINT32_C(4), r.regs.esi)) { return r; }
    zg116_completed(&r, pc);

    /* I17 +042 MOV dword[ESI],0 */
    pc = zg116_add32(wrapper, UINT32_C(42));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_RAM,
        r.regs.esi, UINT32_C(4), UINT32_C(0))) { return r; }
    zg116_completed(&r, pc);

    /* I18 +048 MOV dword[ESI+4],0 */
    pc = zg116_add32(wrapper, UINT32_C(48));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_RAM,
        zg116_add32(r.regs.esi, UINT32_C(4)), UINT32_C(4), UINT32_C(0))) { return r; }
    zg116_completed(&r, pc);

    /* I19 +055 CALL sentinel */
    pc = zg116_add32(wrapper, UINT32_C(55));
    if (!zg116_call_inner(&r, &hooks, wrapper, pc, UINT32_C(1),
        ZG116_ROLE_SENTINEL, sentinel_target,
        zg116_add32(wrapper, UINT32_C(60)))) { return r; }
    zg116_completed(&r, pc);

    /* I20 +060 MOV [ESI],EAX */
    pc = zg116_add32(wrapper, UINT32_C(60));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_RAM,
        r.regs.esi, UINT32_C(4), r.regs.eax)) { return r; }
    zg116_completed(&r, pc);

    /* I21 +062 MOV ECX,[EBP+8] */
    pc = zg116_add32(wrapper, UINT32_C(62));
    if (!zg116_read_word(&r, &hooks, pc, ZG116_SPACE_RAM,
        zg116_add32(r.regs.ebp, UINT32_C(8)), &r.regs.ecx)) { return r; }
    zg116_completed(&r, pc);

    /* I22 +065 PUSH dword[EBP+8]: fresh read precedes ESP decrement */
    pc = zg116_add32(wrapper, UINT32_C(65));
    if (!zg116_read_word(&r, &hooks, pc, ZG116_SPACE_RAM,
        zg116_add32(r.regs.ebp, UINT32_C(8)), &value)) { return r; }
    if (!zg116_push_word(&r, &hooks, pc, value)) { return r; }
    zg116_completed(&r, pc);

    /* I23 +068 MOV EAX,[EAX] */
    pc = zg116_add32(wrapper, UINT32_C(68));
    if (!zg116_read_word(&r, &hooks, pc, ZG116_SPACE_RAM,
        r.regs.eax, &r.regs.eax)) { return r; }
    zg116_completed(&r, pc);

    /* I24 +070 MOV dword[EBP-4],0 */
    pc = zg116_add32(wrapper, UINT32_C(70));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_RAM,
        zg116_sub32(r.regs.ebp, UINT32_C(4)), UINT32_C(4), UINT32_C(0))) { return r; }
    zg116_completed(&r, pc);

    /* I25 +077 MOV ECX,[ECX] */
    pc = zg116_add32(wrapper, UINT32_C(77));
    if (!zg116_read_word(&r, &hooks, pc, ZG116_SPACE_RAM,
        r.regs.ecx, &r.regs.ecx)) { return r; }
    zg116_completed(&r, pc);

    /* I26 +079 PUSH ECX */
    pc = zg116_add32(wrapper, UINT32_C(79));
    if (!zg116_push_word(&r, &hooks, pc, r.regs.ecx)) { return r; }
    zg116_completed(&r, pc);

    /* I27 +080 MOV byte[EBP-4],1: exactly one write1, no read */
    pc = zg116_add32(wrapper, UINT32_C(80));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_RAM,
        zg116_sub32(r.regs.ebp, UINT32_C(4)), UINT32_C(1), UINT32_C(1))) { return r; }
    zg116_completed(&r, pc);

    /* I28 +084 MOV EDX,[ECX] */
    pc = zg116_add32(wrapper, UINT32_C(84));
    if (!zg116_read_word(&r, &hooks, pc, ZG116_SPACE_RAM,
        r.regs.ecx, &r.regs.edx)) { return r; }
    zg116_completed(&r, pc);

    /* I29 +086 MOV ECX,ESI */
    pc = zg116_add32(wrapper, UINT32_C(86));
    r.regs.ecx = r.regs.esi;
    zg116_completed(&r, pc);

    /* I30 +088 PUSH EDX */
    pc = zg116_add32(wrapper, UINT32_C(88));
    if (!zg116_push_word(&r, &hooks, pc, r.regs.edx)) { return r; }
    zg116_completed(&r, pc);

    /* I31 +089 PUSH EAX */
    pc = zg116_add32(wrapper, UINT32_C(89));
    if (!zg116_push_word(&r, &hooks, pc, r.regs.eax)) { return r; }
    zg116_completed(&r, pc);

    /* I32 +090 CALL range */
    pc = zg116_add32(wrapper, UINT32_C(90));
    if (!zg116_call_inner(&r, &hooks, wrapper, pc, UINT32_C(2),
        ZG116_ROLE_RANGE, range_target,
        zg116_add32(wrapper, UINT32_C(95)))) { return r; }
    zg116_completed(&r, pc);

    /* I33 +095 MOV ECX,[EBP-0ch] */
    pc = zg116_add32(wrapper, UINT32_C(95));
    if (!zg116_read_word(&r, &hooks, pc, ZG116_SPACE_RAM,
        zg116_sub32(r.regs.ebp, UINT32_C(12)), &r.regs.ecx)) { return r; }
    zg116_completed(&r, pc);

    /* I34 +098 MOV EAX,ESI */
    pc = zg116_add32(wrapper, UINT32_C(98));
    r.regs.eax = r.regs.esi;
    zg116_completed(&r, pc);

    /* I35 +100 POP EDI */
    pc = zg116_add32(wrapper, UINT32_C(100));
    if (!zg116_pop_word(&r, &hooks, pc, &r.regs.edi)) { return r; }
    zg116_completed(&r, pc);

    /* I36 +101 POP ESI */
    pc = zg116_add32(wrapper, UINT32_C(101));
    if (!zg116_pop_word(&r, &hooks, pc, &r.regs.esi)) { return r; }
    zg116_completed(&r, pc);

    /* I37 +102 MOV FS:[0],ECX */
    pc = zg116_add32(wrapper, UINT32_C(102));
    if (!zg116_write_value(&r, &hooks, pc, ZG116_SPACE_LOGICAL_FS,
        UINT32_C(0), UINT32_C(4), r.regs.ecx)) { return r; }
    zg116_completed(&r, pc);

    /* I38 +109 POP EBX */
    pc = zg116_add32(wrapper, UINT32_C(109));
    if (!zg116_pop_word(&r, &hooks, pc, &r.regs.ebx)) { return r; }
    zg116_completed(&r, pc);

    /* I39 +110 MOV ESP,EBP */
    pc = zg116_add32(wrapper, UINT32_C(110));
    r.regs.esp = r.regs.ebp;
    zg116_completed(&r, pc);

    /* I40 +112 POP EBP */
    pc = zg116_add32(wrapper, UINT32_C(112));
    if (!zg116_pop_word(&r, &hooks, pc, &r.regs.ebp)) { return r; }
    zg116_completed(&r, pc);

    /* I41 +113 RET 4: fresh actualESP cell; publish only after success */
    pc = zg116_add32(wrapper, UINT32_C(113));
    if (!zg116_read_word(&r, &hooks, pc, ZG116_SPACE_RAM,
        r.regs.esp, &value)) { return r; }
    r.regs.esp = zg116_add32(r.regs.esp, UINT32_C(8));
    r.final_target_present = UINT32_C(1);
    r.final_target32 = value;
    r.final_cleanup_present = UINT32_C(1);
    r.final_cleanup32 = UINT32_C(4);
    r.state32 = ZG116_RETURNED;
    r.reason32 = ZG116_REASON_NONE;
    zg116_completed(&r, pc);
    return r;
}
