#include "zg116la_fe0_range.h"

#define LA76_CLONE_TARGET UINT32_C(0x100075e0)
#define LA76_CLONE_CONTINUATION UINT32_C(0x1000772a)
#define LA76_PROVIDER_TARGET UINT32_C(0x1000a130)
#define LA76_PAYLOAD_TARGET UINT32_C(0x10008610)
#define LA76_OPERATOR_NEW_THUNK UINT32_C(0x101bca28)
#define LA76_FAILURE_IAT UINT32_C(0x101d7274)
#define LA76_LENGTH_IAT UINT32_C(0x101d7270)
#define LA76_LENGTH_CALL_PC UINT32_C(0x10007740)
#define LA76_LENGTH_CONTINUATION UINT32_C(0x10007746)
#define LA76_HANDLER UINT32_C(0x101be1d0)

typedef enum la76_phase {
    LA76_INITIAL, LA76_LAST, LA76_POSITION, LA76_PREV, LA76_CLONE,
    LA76_COUNT, LA76_WRITE_COUNT, LA76_WRITE_POSITION, LA76_NODE_PREV,
    LA76_WRITE_PREDECESSOR, LA76_RELOAD_CURRENT, LA76_NEXT,
    LA76_WRITE_CURRENT, LA76_DONE
} la76_phase;

typedef struct la76_frame {
    zg116la_bridge bridge;
    uint32_t q32;
    uint32_t argument_base32;
    la76_phase phase;
    uint32_t initial_valid;
    uint32_t initial_observed32;
    uint32_t current, position, previous, payload, count, node;
    uint32_t predecessor, tail_current;
    zg_w154_call active_call;
    zg116_xmm0 active_xmm0;
} la76_frame;

static const zg116la_identity la76_identity = {
    ZG116_ENTRY_7FE0, UINT32_C(2), ZG116_ROLE_RANGE,
    UINT32_C(0x1000803a), UINT32_C(0x100076e0), UINT32_C(0x1000803f)
};

static uint32_t la76_add(uint32_t a, uint32_t b)
{
    return (uint32_t)((unsigned long long)a + (unsigned long long)b);
}

static zg_w154_regs la76_to_w(zg116_regs8 r)
{
    zg_w154_regs out;
    out.eax32 = r.eax; out.ecx32 = r.ecx; out.edx32 = r.edx;
    out.ebx32 = r.ebx; out.esp32 = r.esp; out.ebp32 = r.ebp;
    out.esi32 = r.esi; out.edi32 = r.edi;
    return out;
}

static zg116_regs8 la76_from_w(zg_w154_regs r)
{
    zg116_regs8 out;
    out.eax = r.eax32; out.ecx = r.ecx32; out.edx = r.edx32;
    out.ebx = r.ebx32; out.esp = r.esp32; out.ebp = r.ebp32;
    out.esi = r.esi32; out.edi = r.edi32;
    return out;
}

static zg_w154_state la76_w_state(la76_frame *f)
{
    uint32_t s = zg116la_bridge_status(&f->bridge);
    if (s == ZG116_SERVICE_OK) { return ZG_W154_RETURNED; }
    if (s == ZG116_SERVICE_STOPPED) { return ZG_W154_NONRETURN; }
    if (s == ZG116_SERVICE_UNKNOWN) { return ZG_W154_UNKNOWN; }
    return ZG_W154_ERROR;
}

static zg_np52_state la76_np_state(la76_frame *f)
{
    uint32_t s = zg116la_bridge_status(&f->bridge);
    if (s == ZG116_SERVICE_OK) { return ZG_NP52_RETURNED; }
    if (s == ZG116_SERVICE_STOPPED) { return ZG_NP52_NONRETURN; }
    return ZG_NP52_ERROR;
}

static zg_nc32_state la76_nc_state(la76_frame *f)
{
    uint32_t s = zg116la_bridge_status(&f->bridge);
    if (s == ZG116_SERVICE_OK) { return ZG_NC32_RETURNED; }
    if (s == ZG116_SERVICE_STOPPED) { return ZG_NC32_NONRETURN; }
    return ZG_NC32_ERROR;
}

static zg_r76_state la76_r76_state(la76_frame *f)
{
    uint32_t s = zg116la_bridge_status(&f->bridge);
    if (s == ZG116_SERVICE_OK) { return ZG_R76_RETURNED; }
    if (s == ZG116_SERVICE_STOPPED) { return ZG_R76_NONRETURN; }
    return ZG_R76_ERROR;
}

static void la76_phase_error(la76_frame *f)
{
    zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
        ZG116LA_REASON_CHILD_RESULT);
}

static uint32_t la76_read(la76_frame *f, uint32_t a, uint32_t *out)
{
    return zg116la_read32(&f->bridge, ZG116_SPACE_RAM, a, out);
}

static uint32_t la76_write(la76_frame *f, uint32_t a, uint32_t value)
{
    return zg116la_write32(&f->bridge, ZG116_SPACE_RAM, a, value);
}

static int la76_push(la76_frame *f, uint32_t value)
{
    f->bridge.regs.esp = la76_add(f->bridge.regs.esp, UINT32_C(0xfffffffc));
    return la76_write(f, f->bridge.regs.esp, value) == ZG116_SERVICE_OK;
}

static int la76_pop(la76_frame *f, uint32_t *out)
{
    uint32_t word = UINT32_C(0);
    if (la76_read(f, f->bridge.regs.esp, &word) != ZG116_SERVICE_OK) {
        return 0;
    }
    f->bridge.regs.esp = la76_add(f->bridge.regs.esp, UINT32_C(4));
    *out = word;
    return 1;
}

static zg_w154_state la76_wread(void *c, uint32_t a, uint32_t *out)
{
    la76_frame *f = (la76_frame *)c;
    (void)la76_read(f, a, out);
    return la76_w_state(f);
}

static zg_w154_state la76_wwrite(void *c, uint32_t a, uint32_t value)
{
    la76_frame *f = (la76_frame *)c;
    (void)la76_write(f, a, value);
    return la76_w_state(f);
}

static zg_w154_state la76_wbyte(void *c, uint32_t a, uint32_t value)
{
    la76_frame *f = (la76_frame *)c;
    (void)zg116la_write8(&f->bridge, ZG116_SPACE_RAM, a, value);
    return la76_w_state(f);
}

static zg_w154_state la76_fsread(void *c, uint32_t *out)
{
    la76_frame *f = (la76_frame *)c;
    (void)zg116la_read32(&f->bridge, ZG116_SPACE_LOGICAL_FS,
        UINT32_C(0), out);
    return la76_w_state(f);
}

static zg_w154_state la76_fswrite(void *c, uint32_t value)
{
    la76_frame *f = (la76_frame *)c;
    (void)zg116la_write32(&f->bridge, ZG116_SPACE_LOGICAL_FS,
        UINT32_C(0), value);
    return la76_w_state(f);
}

static zg116la_machine_call la76_import_call(la76_frame *f,
    uint32_t seam, uint32_t target, uint32_t call_pc,
    uint32_t continuation, uint32_t bytes)
{
    zg116la_machine_call request = {0};
    request.version32 = ZG116LA_VERSION;
    request.seam32 = seam;
    request.ordinal32 = UINT32_C(1);
    request.target32 = target;
    request.call_pc32 = call_pc;
    request.continuation_present = UINT32_C(1);
    request.continuation32 = continuation;
    request.request_bytes32 = bytes;
    request.entry_present = UINT32_C(1);
    request.entry = f->bridge.regs;
    request.xmm0_entry = f->bridge.xmm0;
    return request;
}

static zg_np52_call_result la76_np_project(la76_frame *f,
    zg116la_machine_call request, zg116la_machine_reply reply, uint32_t state)
{
    zg_np52_call_result out = {0};
    out.state = la76_np_state(f);
    if (state != ZG116_SERVICE_OK) { return out; }
    if (reply.returned.esp != la76_add(request.entry.esp, UINT32_C(4))) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_UNKNOWN,
            ZG116LA_REASON_MACHINE_REPLY);
        out.state = ZG_NP52_ERROR;
        return out;
    }
    out.state = ZG_NP52_RETURNED;
    out.eax32 = reply.returned.eax;
    out.normal_abi_preserved = UINT32_C(1);
    return out;
}

static zg_np52_call_result la76_allocate(void *c, uint32_t bytes)
{
    la76_frame *f = (la76_frame *)c;
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    uint32_t state;
    if (bytes != UINT32_C(36) || !la76_push(f, bytes) ||
        !la76_push(f, UINT32_C(0x1000a137))) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return la76_np_project(f, request, reply, ZG116_SERVICE_INVALID);
    }
    request = la76_import_call(f, ZG116LA_SEAM_ALLOCATE,
        LA76_OPERATOR_NEW_THUNK, UINT32_C(0x1000a132),
        UINT32_C(0x1000a137), bytes);
    state = zg116la_invoke_machine(&f->bridge, request, &reply);
    if (state == ZG116_SERVICE_OK) {
        f->bridge.regs.esp = la76_add(f->bridge.regs.esp, UINT32_C(4));
    }
    return la76_np_project(f, request, reply, state);
}

static zg_np52_call_result la76_failure(void *c)
{
    la76_frame *f = (la76_frame *)c;
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    uint32_t target = UINT32_C(0);
    uint32_t state;
    if (la76_read(f, LA76_FAILURE_IAT, &target) != ZG116_SERVICE_OK ||
        !la76_push(f, UINT32_C(0x1000a144))) {
        return la76_np_project(f, request, reply,
            zg116la_bridge_status(&f->bridge));
    }
    request = la76_import_call(f, ZG116LA_SEAM_FAILURE_IMPORT,
        target, UINT32_C(0x1000a13e), UINT32_C(0x1000a144), UINT32_C(0));
    state = zg116la_invoke_machine(&f->bridge, request, &reply);
    return la76_np_project(f, request, reply, state);
}

static zg_np52_state la76_argument(void *c, uint32_t index, uint32_t *out)
{
    la76_frame *f = (la76_frame *)c;
    if ((index != UINT32_C(1) && index != UINT32_C(2)) || out == 0) {
        la76_phase_error(f);
        return la76_np_state(f);
    }
    (void)la76_read(f, la76_add(f->bridge.regs.esp,
        index * UINT32_C(4)), out);
    return la76_np_state(f);
}

static zg_np52_state la76_pwrite(void *c, uint32_t a, uint32_t value)
{
    la76_frame *f = (la76_frame *)c;
    (void)la76_write(f, a, value);
    return la76_np_state(f);
}

static zg_nc32_state la76_nread(void *c, uint32_t a, uint32_t *out)
{
    la76_frame *f = (la76_frame *)c;
    (void)la76_read(f, a, out);
    return la76_nc_state(f);
}

static zg_nc32_state la76_nwrite(void *c, uint32_t a, uint32_t value)
{
    la76_frame *f = (la76_frame *)c;
    (void)la76_write(f, a, value);
    return la76_nc_state(f);
}

static zg_nc32_state la76_xadd(void *c, uint32_t a, uint32_t delta,
    uint32_t *old)
{
    la76_frame *f = (la76_frame *)c;
    uint32_t value = UINT32_C(0);
    if (old == 0 || la76_read(f, a, &value) != ZG116_SERVICE_OK) {
        if (old == 0) { la76_phase_error(f); }
        return la76_nc_state(f);
    }
    /* Selected synchronous logical RMW only; no physical LOCK claim. */
    if (la76_write(f, a, la76_add(value, delta)) != ZG116_SERVICE_OK) {
        return la76_nc_state(f);
    }
    *old = value;
    return ZG_NC32_RETURNED;
}

static zg_nc32_opaque_result la76_slot(la76_frame *f, uint32_t object,
    uint32_t table, uint32_t target, int slot4)
{
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    zg_nc32_opaque_result out = {0};
    uint32_t state;
    request.version32 = ZG116LA_VERSION;
    request.seam32 = slot4 ? ZG116LA_SEAM_SLOT4 : ZG116LA_SEAM_SLOT8;
    request.ordinal32 = UINT32_C(2);
    request.target32 = target;
    request.object32 = object;
    request.fresh_table32 = table;
    request.fresh_target32 = target;
    /* entry_present remains0: this legacy API has no actual slot-entry GPR. */
    state = zg116la_invoke_machine(&f->bridge, request, &reply);
    out.state = la76_nc_state(f);
    if (state == ZG116_SERVICE_OK) {
        out.state = ZG_NC32_RETURNED;
        out.normal_abi_preserved = UINT32_C(1);
    }
    return out;
}

static zg_nc32_opaque_result la76_slot4(void *c, uint32_t object,
    uint32_t table, uint32_t target)
{
    return la76_slot((la76_frame *)c, object, table, target, 1);
}

static zg_nc32_opaque_result la76_slot8(void *c, uint32_t object,
    uint32_t table, uint32_t target)
{
    return la76_slot((la76_frame *)c, object, table, target, 0);
}

static zg_w154_call_result la76_bad_w(la76_frame *f)
{
    zg_w154_call_result out = {0};
    out.state = la76_w_state(f);
    return out;
}

static zg_w154_call_result la76_finish(la76_frame *f, uint32_t value)
{
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    zg_w154_call_result out = {0};
    uint32_t actual_return = UINT32_C(0);
    uint32_t expected_esp;
    uint32_t state;
    request.version32 = ZG116LA_VERSION;
    request.seam32 = ZG116LA_SEAM_FINISH_NORMAL;
    request.ordinal32 = f->active_call.ordinal32;
    request.target32 = f->active_call.target32;
    request.continuation_present = UINT32_C(1);
    request.continuation32 = f->active_call.continuation32;
    request.core_return_value32 = value;
    request.callee_ret_cleanup32 = f->active_call.callee_ret_cleanup32;
    request.entry_present = UINT32_C(1);
    request.entry = la76_from_w(f->active_call.entry);
    request.xmm0_entry = f->active_xmm0;
    state = zg116la_invoke_machine(&f->bridge, request, &reply);
    if (state != ZG116_SERVICE_OK) { return la76_bad_w(f); }
    /* The seam does not replace the callee's own late fresh return-cell read. */
    if (la76_read(f, f->active_call.entry.esp32,
            &actual_return) != ZG116_SERVICE_OK) {
        return la76_bad_w(f);
    }
    expected_esp = la76_add(f->active_call.entry.esp32,
        la76_add(UINT32_C(4), f->active_call.callee_ret_cleanup32));
    if (reply.returned.eax != value ||
        reply.returned_to32 != actual_return ||
        reply.returned.esp != expected_esp) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_UNKNOWN,
            ZG116LA_REASON_MACHINE_REPLY);
        return la76_bad_w(f);
    }
    out.state = ZG_W154_RETURNED;
    out.normal_abi_preserved = UINT32_C(1);
    out.returned_to32 = reply.returned_to32;
    out.returned = la76_to_w(reply.returned);
    return out;
}

static zg_w154_call_result la76_first(void *c, zg_w154_call call)
{
    la76_frame *f = (la76_frame *)c;
    zg_np52_services ops;
    zg_np52_result result;
    if (call.ordinal32 != UINT32_C(1) || call.target32 != LA76_PROVIDER_TARGET ||
        call.callee_ret_cleanup32 != UINT32_C(8)) {
        la76_phase_error(f);
        return la76_bad_w(f);
    }
    f->active_call = call;
    f->active_xmm0 = f->bridge.xmm0;
    f->bridge.regs = la76_from_w(call.entry);
    ops.context = f;
    ops.allocate = la76_allocate;
    ops.failure_import = la76_failure;
    ops.read_argument32 = la76_argument;
    ops.write32 = la76_pwrite;
    result = zg_np52_provider_candidate(&ops);
    if (result.state != ZG_NP52_RETURNED) {
        if (zg116la_bridge_status(&f->bridge) == ZG116_SERVICE_OK) {
            zg116la_bridge_latch(&f->bridge,
                result.state == ZG_NP52_NONRETURN ? ZG116_SERVICE_STOPPED :
                    ZG116_SERVICE_INVALID, ZG116LA_REASON_CHILD_RESULT);
        }
        return la76_bad_w(f);
    }
    if (result.reason != ZG_NP52_REASON_NONE ||
        result.stage != ZG_NP52_STAGE_DONE) {
        la76_phase_error(f);
        return la76_bad_w(f);
    }
    return la76_finish(f, result.return_value32);
}

static zg_w154_call_result la76_second(void *c, zg_w154_call call)
{
    la76_frame *f = (la76_frame *)c;
    zg_nc32_services ops;
    zg_nc32_result result;
    uint32_t source = UINT32_C(0);
    if (call.ordinal32 != UINT32_C(2) || call.target32 != LA76_PAYLOAD_TARGET ||
        call.callee_ret_cleanup32 != UINT32_C(4)) {
        la76_phase_error(f);
        return la76_bad_w(f);
    }
    f->active_call = call;
    f->active_xmm0 = f->bridge.xmm0;
    f->bridge.regs = la76_from_w(call.entry);
    if (la76_read(f, la76_add(call.entry.esp32, UINT32_C(4)),
            &source) != ZG116_SERVICE_OK) {
        return la76_bad_w(f);
    }
    ops.context = f; ops.read32 = la76_nread; ops.write32 = la76_nwrite;
    ops.atomic_xadd32 = la76_xadd;
    ops.call_slot4 = la76_slot4; ops.call_slot8 = la76_slot8;
    result = zg_nc32_copy7_candidate(&ops, call.entry.ecx32, source);
    if (result.state != ZG_NC32_RETURNED) {
        if (zg116la_bridge_status(&f->bridge) == ZG116_SERVICE_OK) {
            zg116la_bridge_latch(&f->bridge,
                result.state == ZG_NC32_NONRETURN ? ZG116_SERVICE_STOPPED :
                    ZG116_SERVICE_INVALID, ZG116LA_REASON_CHILD_RESULT);
        }
        return la76_bad_w(f);
    }
    if (result.reason != ZG_NC32_REASON_NONE ||
        result.stage != ZG_NC32_STAGE_DONE) {
        la76_phase_error(f);
        return la76_bad_w(f);
    }
    return la76_finish(f, result.return_value32);
}

static zg_r76_state la76_range_read(void *c, uint32_t a, uint32_t *out)
{
    la76_frame *f = (la76_frame *)c;
    uint32_t expected;
    switch (f->phase) {
    case LA76_INITIAL: case LA76_RELOAD_CURRENT:
        expected = la76_add(f->argument_base32, UINT32_C(4)); break;
    case LA76_LAST: expected = la76_add(f->argument_base32, UINT32_C(8)); break;
    case LA76_POSITION: expected = f->argument_base32; break;
    case LA76_PREV: expected = la76_add(f->position, UINT32_C(4)); break;
    case LA76_COUNT: expected = la76_add(f->q32, UINT32_C(4)); break;
    case LA76_NODE_PREV: expected = la76_add(f->node, UINT32_C(4)); break;
    case LA76_NEXT: expected = f->tail_current; break;
    default: la76_phase_error(f); return la76_r76_state(f);
    }
    if (a != expected) { la76_phase_error(f); return la76_r76_state(f); }
    if (f->phase == LA76_INITIAL) {
        if (f->initial_valid != UINT32_C(1) || out == 0) {
            la76_phase_error(f);
            return la76_r76_state(f);
        }
        /* Consume the one real 0x100076fb observation; never read it twice. */
        f->current = f->initial_observed32;
        f->initial_valid = UINT32_C(0);
        f->bridge.regs.eax = f->current;
        f->phase = LA76_LAST;
        *out = f->current;
        return ZG_R76_RETURNED;
    }
    if (la76_read(f, a, out) != ZG116_SERVICE_OK) {
        return la76_r76_state(f);
    }
    switch (f->phase) {
    case LA76_LAST:
        f->phase = f->current == *out ? LA76_DONE : LA76_POSITION; break;
    case LA76_POSITION:
        f->position = *out;
        f->payload = la76_add(f->current, UINT32_C(8));
        f->bridge.regs.esi = f->position;
        f->bridge.regs.eax = f->payload;
        if (!la76_push(f, f->payload)) { return la76_r76_state(f); }
        f->bridge.regs.ecx = f->bridge.regs.edi;
        f->phase = LA76_PREV;
        break;
    case LA76_PREV:
        f->previous = *out;
        if (!la76_push(f, f->previous) || !la76_push(f, f->position)) {
            return la76_r76_state(f);
        }
        f->phase = LA76_CLONE;
        break;
    case LA76_COUNT:
        f->count = *out;
        f->bridge.regs.eax = f->count;
        f->bridge.regs.ecx = la76_add(f->bridge.regs.ecx,
            (uint32_t)(UINT32_C(0) - f->count));
        f->phase = LA76_WRITE_COUNT;
        break;
    case LA76_NODE_PREV:
        f->predecessor = *out;
        f->bridge.regs.eax = f->predecessor;
        f->phase = LA76_WRITE_PREDECESSOR;
        break;
    case LA76_RELOAD_CURRENT:
        f->tail_current = *out;
        f->bridge.regs.eax = f->tail_current;
        f->phase = LA76_NEXT;
        break;
    case LA76_NEXT:
        f->bridge.regs.eax = *out;
        f->phase = LA76_WRITE_CURRENT;
        break;
    default: la76_phase_error(f); return la76_r76_state(f);
    }
    return ZG_R76_RETURNED;
}

static zg_r76_state la76_range_write(void *c, uint32_t a, uint32_t value)
{
    la76_frame *f = (la76_frame *)c;
    uint32_t expected;
    la76_phase next;
    switch (f->phase) {
    case LA76_WRITE_COUNT:
        expected = la76_add(f->q32, UINT32_C(4));
        next = LA76_WRITE_POSITION;
        f->bridge.regs.eax = value; /* INC precedes the committed store. */
        break;
    case LA76_WRITE_POSITION:
        expected = la76_add(f->position, UINT32_C(4)); next = LA76_NODE_PREV; break;
    case LA76_WRITE_PREDECESSOR:
        expected = f->predecessor; next = LA76_RELOAD_CURRENT; break;
    case LA76_WRITE_CURRENT:
        expected = la76_add(f->argument_base32, UINT32_C(4)); next = LA76_LAST; break;
    default: la76_phase_error(f); return la76_r76_state(f);
    }
    if (a != expected) { la76_phase_error(f); return la76_r76_state(f); }
    if (la76_write(f, a, value) != ZG116_SERVICE_OK) {
        return la76_r76_state(f);
    }
    if (f->phase == LA76_WRITE_CURRENT) { f->current = value; }
    f->phase = next;
    return ZG_R76_RETURNED;
}

static zg_r76_clone_result la76_clone(void *c, uint32_t q,
    uint32_t position, uint32_t previous, uint32_t payload)
{
    la76_frame *f = (la76_frame *)c;
    zg_r76_clone_result out = {ZG_R76_ERROR, 0, 0};
    zg_w154_services ops;
    zg_w154_result result;
    zg_w154_regs entry;
    if (f->phase != LA76_CLONE || q != f->q32 || position != f->position ||
        previous != f->previous || payload != f->payload) {
        la76_phase_error(f);
        out.state = la76_r76_state(f);
        return out;
    }
    if (!la76_push(f, LA76_CLONE_CONTINUATION)) {
        out.state = la76_r76_state(f);
        return out;
    }
    entry = la76_to_w(f->bridge.regs);
    ops.context = f; ops.read32 = la76_wread; ops.write32 = la76_wwrite;
    ops.write8 = la76_wbyte; ops.read_fs_head32 = la76_fsread;
    ops.write_fs_head32 = la76_fswrite;
    ops.call_first = la76_first; ops.call_second = la76_second;
    result = zg_wrapper154_candidate(&ops, ZG_W154_75E0, entry);
    if (result.state != ZG_W154_RETURNED) {
        if (zg116la_bridge_status(&f->bridge) == ZG116_SERVICE_OK) {
            uint32_t state = ZG116_SERVICE_INVALID;
            if (result.state == ZG_W154_NONRETURN) { state = ZG116_SERVICE_STOPPED; }
            else if (result.state == ZG_W154_UNKNOWN ||
                (result.state == ZG_W154_ERROR &&
                    (result.reason == ZG_W154_REASON_ABI_UNCONFIRMED ||
                        result.reason == ZG_W154_REASON_ABI_MISMATCH))) {
                state = ZG116_SERVICE_UNKNOWN;
            }
            zg116la_bridge_latch(&f->bridge, state, ZG116LA_REASON_CHILD_RESULT);
        }
        out.state = la76_r76_state(f);
        return out;
    }
    if (result.reason != ZG_W154_REASON_NONE ||
        result.phase != ZG_W154_PHASE_DONE ||
        result.wrapper_ret_cleanup32 != UINT32_C(12) ||
        result.return_target32 != LA76_CLONE_CONTINUATION ||
        result.registers32.esp32 != la76_add(f->argument_base32, UINT32_C(0xffffffd4)) ||
        result.registers32.ebp32 != la76_add(f->argument_base32, UINT32_C(0xfffffff8)) ||
        result.registers32.ebx32 != entry.ebx32 ||
        result.registers32.esi32 != entry.esi32 ||
        result.registers32.edi32 != entry.edi32 ||
        result.return_value32 != result.registers32.eax32) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_UNKNOWN,
            ZG116LA_REASON_CHILD_RESULT);
        out.state = la76_r76_state(f);
        return out;
    }
    f->bridge.regs = la76_from_w(result.registers32);
    f->node = result.return_value32;
    f->bridge.regs.edx = f->node;
    f->bridge.regs.ecx = ZG_R76_COUNT_LIMIT32;
    f->phase = LA76_COUNT;
    out.state = ZG_R76_RETURNED;
    out.eax32 = f->node;
    out.normal_abi_preserved = UINT32_C(1);
    return out;
}

static zg_r76_state la76_length(void *c, uint32_t message)
{
    la76_frame *f = (la76_frame *)c;
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    uint32_t target = UINT32_C(0);
    if (f->phase != LA76_WRITE_COUNT || f->count != ZG_R76_COUNT_LIMIT32 ||
        message != ZG_R76_LENGTH_MESSAGE32) {
        la76_phase_error(f);
        return la76_r76_state(f);
    }
    if (!la76_push(f, message) ||
        la76_read(f, LA76_LENGTH_IAT, &target) != ZG116_SERVICE_OK ||
        !la76_push(f, LA76_LENGTH_CONTINUATION)) {
        return la76_r76_state(f);
    }
    request = la76_import_call(f, ZG116LA_SEAM_LENGTH_ERROR,
        target, LA76_LENGTH_CALL_PC, LA76_LENGTH_CONTINUATION, UINT32_C(0));
    if (request.entry.ecx != UINT32_C(0) ||
        request.entry.eax != ZG_R76_COUNT_LIMIT32 ||
        request.entry.edx != f->node) {
        la76_phase_error(f);
        return la76_r76_state(f);
    }
    request.message32 = message;
    request.ordinal32 = UINT32_C(2);
    (void)zg116la_invoke_machine(&f->bridge, request, &reply);
    /* RETURNED reaches the unchanged range76 unsupported frontier. */
    return la76_r76_state(f);
}

static int la76_prologue(la76_frame *f)
{
    if (!la76_push(f, f->bridge.regs.ebp)) { return 0; }
    f->bridge.regs.ebp = f->bridge.regs.esp;
    if (!la76_push(f, UINT32_C(0xffffffff)) ||
        !la76_push(f, LA76_HANDLER)) { return 0; }
    if (zg116la_read32(&f->bridge, ZG116_SPACE_LOGICAL_FS, UINT32_C(0),
            &f->bridge.regs.eax) != ZG116_SERVICE_OK) { return 0; }
    if (!la76_push(f, f->bridge.regs.eax) ||
        zg116la_write32(&f->bridge, ZG116_SPACE_LOGICAL_FS, UINT32_C(0),
            f->bridge.regs.esp) != ZG116_SERVICE_OK) { return 0; }
    f->bridge.regs.esp = la76_add(f->bridge.regs.esp, UINT32_C(0xfffffff4));
    /* site 0x100076fb: the one actual initial-current read. */
    if (la76_read(f, la76_add(f->bridge.regs.ebp, UINT32_C(12)),
            &f->bridge.regs.eax) != ZG116_SERVICE_OK) { return 0; }
    f->initial_observed32 = f->bridge.regs.eax;
    f->initial_valid = UINT32_C(1);
    if (!la76_push(f, f->bridge.regs.ebx) ||
        !la76_push(f, f->bridge.regs.esi) ||
        !la76_push(f, f->bridge.regs.edi)) { return 0; }
    f->bridge.regs.edi = f->bridge.regs.ecx;
    if (la76_write(f, la76_add(f->bridge.regs.ebp, UINT32_C(0xfffffff0)),
            f->bridge.regs.esp) != ZG116_SERVICE_OK ||
        la76_write(f, la76_add(f->bridge.regs.ebp, UINT32_C(0xffffffe8)),
            f->bridge.regs.edi) != ZG116_SERVICE_OK ||
        la76_write(f, la76_add(f->bridge.regs.ebp, UINT32_C(0xffffffec)),
            f->bridge.regs.eax) != ZG116_SERVICE_OK ||
        la76_write(f, la76_add(f->bridge.regs.ebp, UINT32_C(0xfffffffc)),
            UINT32_C(0)) != ZG116_SERVICE_OK) { return 0; }
    return 1;
}

static int la76_epilogue(la76_frame *f, zg116_lower_reply *reply)
{
    uint32_t target = UINT32_C(0);
    if (la76_read(f, la76_add(f->bridge.regs.ebp, UINT32_C(0xfffffff4)),
            &f->bridge.regs.ecx) != ZG116_SERVICE_OK) { return 0; }
    if (!la76_pop(f, &f->bridge.regs.edi) ||
        !la76_pop(f, &f->bridge.regs.esi)) { return 0; }
    if (zg116la_write32(&f->bridge, ZG116_SPACE_LOGICAL_FS, UINT32_C(0),
            f->bridge.regs.ecx) != ZG116_SERVICE_OK) { return 0; }
    if (!la76_pop(f, &f->bridge.regs.ebx)) { return 0; }
    f->bridge.regs.esp = f->bridge.regs.ebp;
    if (!la76_pop(f, &f->bridge.regs.ebp) ||
        la76_read(f, f->bridge.regs.esp, &target) != ZG116_SERVICE_OK) {
        return 0;
    }
    f->bridge.regs.esp = la76_add(f->bridge.regs.esp, UINT32_C(20));
    zg116la_lower_returned(reply, f->bridge.regs, f->bridge.xmm0,
        target, UINT32_C(16));
    return 1;
}

void zg116la_fe0_range(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply)
{
    la76_frame f = {0};
    zg_r76_services ops;
    zg_r76_result result;
    if (reply == 0) { return; }
    zg116la_lower_clear(reply);
    if (!zg116la_request_matches(&request, &la76_identity)) {
        reply->state32 = ZG116_LOWER_INVALID;
        reply->reason32 = ZG116LA_REASON_REQUEST_IDENTITY;
        return;
    }
    if (!zg116la_bridge_begin(&f.bridge, (const zg116la_config *)user,
            memory, &request)) {
        reply->state32 = ZG116_LOWER_INVALID;
        reply->reason32 = ZG116LA_REASON_BAD_INTERFACE;
        return;
    }
    f.q32 = request.entry.ecx;
    f.argument_base32 = la76_add(request.entry.esp, UINT32_C(4));
    f.phase = LA76_INITIAL;
    if (!la76_prologue(&f)) {
        zg116la_lower_from_bridge(&f.bridge, reply);
        return;
    }
    ops.context = &f; ops.read32 = la76_range_read;
    ops.write32 = la76_range_write; ops.clone = la76_clone;
    ops.length_error = la76_length;
    result = zg_range76e0_normal(&ops, f.q32, f.argument_base32);
    if (result.state != ZG_R76_RETURNED) {
        if (zg116la_bridge_status(&f.bridge) == ZG116_SERVICE_OK) {
            uint32_t state = result.state == ZG_R76_NONRETURN ?
                ZG116_SERVICE_STOPPED : ZG116_SERVICE_INVALID;
            if (result.reason == ZG_R76_REASON_ABI_UNCONFIRMED ||
                result.reason == ZG_R76_REASON_LENGTH_ERROR_RETURN_FRONTIER) {
                state = ZG116_SERVICE_UNKNOWN;
            }
            zg116la_bridge_latch(&f.bridge, state, ZG116LA_REASON_CHILD_RESULT);
        }
        zg116la_lower_from_bridge(&f.bridge, reply);
        return;
    }
    if (zg116la_bridge_status(&f.bridge) != ZG116_SERVICE_OK) {
        zg116la_lower_from_bridge(&f.bridge, reply);
        return;
    }
    if (result.reason != ZG_R76_REASON_NONE || result.stage != ZG_R76_STAGE_DONE ||
        f.phase != LA76_DONE || f.initial_valid != UINT32_C(0) ||
        f.bridge.regs.esp != la76_add(f.argument_base32, UINT32_C(0xffffffd4)) ||
        f.bridge.regs.ebp != la76_add(f.argument_base32, UINT32_C(0xfffffff8)) ||
        f.bridge.regs.edi != f.q32) {
        zg116la_bridge_latch(&f.bridge, ZG116_SERVICE_UNKNOWN,
            ZG116LA_REASON_CHILD_RESULT);
        zg116la_lower_from_bridge(&f.bridge, reply);
        return;
    }
    f.bridge.regs.eax = result.return_value32;
    if (!la76_epilogue(&f, reply)) {
        zg116la_lower_from_bridge(&f.bridge, reply);
    }
}
