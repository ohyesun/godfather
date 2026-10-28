#include "zg116la_e4_range.h"

#define LE4_PROVIDER_TARGET UINT32_C(0x1002ee30)
#define LE4_PAYLOAD_TARGET UINT32_C(0x10024470)
#define LE4_OPERATOR_NEW_THUNK UINT32_C(0x101bca28)
#define LE4_FAILURE_IAT UINT32_C(0x101d7274)

typedef struct le4_frame {
    zg116la_bridge bridge;
    zg_w154_call active_call;
    zg116_xmm0 active_xmm0;
} le4_frame;

static const zg116la_identity le4_identity = {
    ZG116_ENTRY_E4C0, UINT32_C(2), ZG116_ROLE_RANGE,
    UINT32_C(0x1001e51a), UINT32_C(0x100101e0), UINT32_C(0x1001e51f)
};

static uint32_t le4_add(uint32_t a, uint32_t b)
{
    return (uint32_t)((unsigned long long)a + (unsigned long long)b);
}

static zg_w154_regs le4_to_w(zg116_regs8 r)
{
    zg_w154_regs out;
    out.eax32 = r.eax; out.ecx32 = r.ecx; out.edx32 = r.edx;
    out.ebx32 = r.ebx; out.esp32 = r.esp; out.ebp32 = r.ebp;
    out.esi32 = r.esi; out.edi32 = r.edi;
    return out;
}

static zg116_regs8 le4_from_w(zg_w154_regs r)
{
    zg116_regs8 out;
    out.eax = r.eax32; out.ecx = r.ecx32; out.edx = r.edx32;
    out.ebx = r.ebx32; out.esp = r.esp32; out.ebp = r.ebp32;
    out.esi = r.esi32; out.edi = r.edi32;
    return out;
}

static zg_w154_state le4_w_state(le4_frame *f)
{
    uint32_t s = zg116la_bridge_status(&f->bridge);
    if (s == ZG116_SERVICE_OK) { return ZG_W154_RETURNED; }
    if (s == ZG116_SERVICE_STOPPED) { return ZG_W154_NONRETURN; }
    if (s == ZG116_SERVICE_UNKNOWN) { return ZG_W154_UNKNOWN; }
    return ZG_W154_ERROR;
}

static zg_np52_state le4_np_state(le4_frame *f)
{
    uint32_t s = zg116la_bridge_status(&f->bridge);
    if (s == ZG116_SERVICE_OK) { return ZG_NP52_RETURNED; }
    if (s == ZG116_SERVICE_STOPPED) { return ZG_NP52_NONRETURN; }
    return ZG_NP52_ERROR;
}

static zg_nc32_state le4_nc_state(le4_frame *f)
{
    uint32_t s = zg116la_bridge_status(&f->bridge);
    if (s == ZG116_SERVICE_OK) { return ZG_NC32_RETURNED; }
    if (s == ZG116_SERVICE_STOPPED) { return ZG_NC32_NONRETURN; }
    return ZG_NC32_ERROR;
}

static uint32_t le4_read(le4_frame *f, uint32_t a, uint32_t *out)
{
    return zg116la_read32(&f->bridge, ZG116_SPACE_RAM, a, out);
}

static uint32_t le4_write(le4_frame *f, uint32_t a, uint32_t value)
{
    return zg116la_write32(&f->bridge, ZG116_SPACE_RAM, a, value);
}

static int le4_push(le4_frame *f, uint32_t value)
{
    f->bridge.regs.esp = le4_add(f->bridge.regs.esp, UINT32_C(0xfffffffc));
    return le4_write(f, f->bridge.regs.esp, value) == ZG116_SERVICE_OK;
}

static zg_w154_state le4_wread(void *c, uint32_t a, uint32_t *out)
{
    le4_frame *f = (le4_frame *)c;
    (void)le4_read(f, a, out);
    return le4_w_state(f);
}

static zg_w154_state le4_wwrite(void *c, uint32_t a, uint32_t value)
{
    le4_frame *f = (le4_frame *)c;
    (void)le4_write(f, a, value);
    return le4_w_state(f);
}

static zg_w154_state le4_wbyte(void *c, uint32_t a, uint32_t value)
{
    le4_frame *f = (le4_frame *)c;
    (void)zg116la_write8(&f->bridge, ZG116_SPACE_RAM, a, value);
    return le4_w_state(f);
}

static zg_w154_state le4_fsread(void *c, uint32_t *out)
{
    le4_frame *f = (le4_frame *)c;
    (void)zg116la_read32(&f->bridge, ZG116_SPACE_LOGICAL_FS,
        UINT32_C(0), out);
    return le4_w_state(f);
}

static zg_w154_state le4_fswrite(void *c, uint32_t value)
{
    le4_frame *f = (le4_frame *)c;
    (void)zg116la_write32(&f->bridge, ZG116_SPACE_LOGICAL_FS,
        UINT32_C(0), value);
    return le4_w_state(f);
}

static zg116la_machine_call le4_import_call(le4_frame *f,
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

static zg_np52_call_result le4_np_project(le4_frame *f,
    zg116la_machine_call request, zg116la_machine_reply reply, uint32_t state)
{
    zg_np52_call_result out = {0};
    out.state = le4_np_state(f);
    if (state != ZG116_SERVICE_OK) { return out; }
    if (reply.returned.esp != le4_add(request.entry.esp, UINT32_C(4))) {
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

static zg_np52_call_result le4_allocate(void *c, uint32_t bytes)
{
    le4_frame *f = (le4_frame *)c;
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    uint32_t state;
    if (bytes != UINT32_C(36) || !le4_push(f, bytes) ||
        !le4_push(f, UINT32_C(0x1002ee37))) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return le4_np_project(f, request, reply, ZG116_SERVICE_INVALID);
    }
    request = le4_import_call(f, ZG116LA_SEAM_ALLOCATE,
        LE4_OPERATOR_NEW_THUNK, UINT32_C(0x1002ee32),
        UINT32_C(0x1002ee37), bytes);
    state = zg116la_invoke_machine(&f->bridge, request, &reply);
    if (state == ZG116_SERVICE_OK) {
        f->bridge.regs.esp = le4_add(f->bridge.regs.esp, UINT32_C(4));
    }
    return le4_np_project(f, request, reply, state);
}

static zg_np52_call_result le4_failure(void *c)
{
    le4_frame *f = (le4_frame *)c;
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    uint32_t target = UINT32_C(0);
    uint32_t state;
    if (le4_read(f, LE4_FAILURE_IAT, &target) != ZG116_SERVICE_OK ||
        !le4_push(f, UINT32_C(0x1002ee44))) {
        return le4_np_project(f, request, reply,
            zg116la_bridge_status(&f->bridge));
    }
    request = le4_import_call(f, ZG116LA_SEAM_FAILURE_IMPORT,
        target, UINT32_C(0x1002ee3e), UINT32_C(0x1002ee44), UINT32_C(0));
    state = zg116la_invoke_machine(&f->bridge, request, &reply);
    return le4_np_project(f, request, reply, state);
}

static zg_np52_state le4_argument(void *c, uint32_t index, uint32_t *out)
{
    le4_frame *f = (le4_frame *)c;
    if ((index != UINT32_C(1) && index != UINT32_C(2)) || out == 0) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        return le4_np_state(f);
    }
    (void)le4_read(f, le4_add(f->bridge.regs.esp,
        index * UINT32_C(4)), out);
    return le4_np_state(f);
}

static zg_np52_state le4_pwrite(void *c, uint32_t a, uint32_t value)
{
    le4_frame *f = (le4_frame *)c;
    (void)le4_write(f, a, value);
    return le4_np_state(f);
}

static zg_nc32_state le4_nread(void *c, uint32_t a, uint32_t *out)
{
    le4_frame *f = (le4_frame *)c;
    (void)le4_read(f, a, out);
    return le4_nc_state(f);
}

static zg_nc32_state le4_nwrite(void *c, uint32_t a, uint32_t value)
{
    le4_frame *f = (le4_frame *)c;
    (void)le4_write(f, a, value);
    return le4_nc_state(f);
}

static zg_nc32_state le4_xadd(void *c, uint32_t a, uint32_t delta,
    uint32_t *old)
{
    le4_frame *f = (le4_frame *)c;
    uint32_t value = UINT32_C(0);
    if (old == 0 || le4_read(f, a, &value) != ZG116_SERVICE_OK) {
        if (old == 0) {
            zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
                ZG116LA_REASON_CHILD_RESULT);
        }
        return le4_nc_state(f);
    }
    /* Synchronous logical RMW only; no physical LOCK or atomicity claim. */
    if (le4_write(f, a, le4_add(value, delta)) != ZG116_SERVICE_OK) {
        return le4_nc_state(f);
    }
    *old = value;
    return ZG_NC32_RETURNED;
}

static zg_nc32_opaque_result le4_slot(le4_frame *f, uint32_t object,
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
    /* No legacy slot-entry GPR/XMM: entry_present and continuation stay zero. */
    state = zg116la_invoke_machine(&f->bridge, request, &reply);
    out.state = le4_nc_state(f);
    if (state == ZG116_SERVICE_OK) {
        out.state = ZG_NC32_RETURNED;
        out.normal_abi_preserved = UINT32_C(1);
    }
    return out;
}

static zg_nc32_opaque_result le4_slot4(void *c, uint32_t object,
    uint32_t table, uint32_t target)
{
    return le4_slot((le4_frame *)c, object, table, target, 1);
}

static zg_nc32_opaque_result le4_slot8(void *c, uint32_t object,
    uint32_t table, uint32_t target)
{
    return le4_slot((le4_frame *)c, object, table, target, 0);
}

static zg_w154_call_result le4_bad_w(le4_frame *f)
{
    zg_w154_call_result out = {0};
    out.state = le4_w_state(f);
    return out;
}

static zg_w154_call_result le4_finish(le4_frame *f, uint32_t value)
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
    request.entry = le4_from_w(f->active_call.entry);
    request.xmm0_entry = f->active_xmm0;
    state = zg116la_invoke_machine(&f->bridge, request, &reply);
    if (state != ZG116_SERVICE_OK) { return le4_bad_w(f); }
    if (le4_read(f, f->active_call.entry.esp32,
            &actual_return) != ZG116_SERVICE_OK) {
        return le4_bad_w(f);
    }
    expected_esp = le4_add(f->active_call.entry.esp32,
        le4_add(UINT32_C(4), f->active_call.callee_ret_cleanup32));
    if (reply.returned.eax != value ||
        reply.returned_to32 != actual_return ||
        reply.returned.esp != expected_esp) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_UNKNOWN,
            ZG116LA_REASON_MACHINE_REPLY);
        return le4_bad_w(f);
    }
    out.state = ZG_W154_RETURNED;
    out.normal_abi_preserved = UINT32_C(1);
    out.returned_to32 = reply.returned_to32;
    out.returned = le4_to_w(reply.returned);
    return out;
}

static zg_w154_call_result le4_first(void *c, zg_w154_call call)
{
    le4_frame *f = (le4_frame *)c;
    zg_np52_services ops;
    zg_np52_result result;
    if (call.ordinal32 != UINT32_C(1) || call.target32 != LE4_PROVIDER_TARGET ||
        call.callee_ret_cleanup32 != UINT32_C(8)) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        return le4_bad_w(f);
    }
    f->active_call = call;
    f->active_xmm0 = f->bridge.xmm0;
    f->bridge.regs = le4_from_w(call.entry);
    ops.context = f;
    ops.allocate = le4_allocate;
    ops.failure_import = le4_failure;
    ops.read_argument32 = le4_argument;
    ops.write32 = le4_pwrite;
    result = zg_np52_provider_candidate(&ops);
    if (result.state != ZG_NP52_RETURNED) {
        if (zg116la_bridge_status(&f->bridge) == ZG116_SERVICE_OK) {
            zg116la_bridge_latch(&f->bridge,
                result.state == ZG_NP52_NONRETURN ? ZG116_SERVICE_STOPPED :
                    ZG116_SERVICE_INVALID, ZG116LA_REASON_CHILD_RESULT);
        }
        return le4_bad_w(f);
    }
    if (result.reason != ZG_NP52_REASON_NONE ||
        result.stage != ZG_NP52_STAGE_DONE) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        return le4_bad_w(f);
    }
    return le4_finish(f, result.return_value32);
}

static zg_w154_call_result le4_second(void *c, zg_w154_call call)
{
    le4_frame *f = (le4_frame *)c;
    zg_nc32_services ops;
    zg_nc32_result result;
    uint32_t source = UINT32_C(0);
    if (call.ordinal32 != UINT32_C(2) || call.target32 != LE4_PAYLOAD_TARGET ||
        call.callee_ret_cleanup32 != UINT32_C(4)) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        return le4_bad_w(f);
    }
    f->active_call = call;
    f->active_xmm0 = f->bridge.xmm0;
    f->bridge.regs = le4_from_w(call.entry);
    if (le4_read(f, le4_add(call.entry.esp32, UINT32_C(4)),
            &source) != ZG116_SERVICE_OK) {
        return le4_bad_w(f);
    }
    ops.context = f; ops.read32 = le4_nread; ops.write32 = le4_nwrite;
    ops.atomic_xadd32 = le4_xadd;
    ops.call_slot4 = le4_slot4; ops.call_slot8 = le4_slot8;
    result = zg_nc32_copy7_candidate(&ops, call.entry.ecx32, source);
    if (result.state != ZG_NC32_RETURNED) {
        if (zg116la_bridge_status(&f->bridge) == ZG116_SERVICE_OK) {
            zg116la_bridge_latch(&f->bridge,
                result.state == ZG_NC32_NONRETURN ? ZG116_SERVICE_STOPPED :
                    ZG116_SERVICE_INVALID, ZG116LA_REASON_CHILD_RESULT);
        }
        return le4_bad_w(f);
    }
    if (result.reason != ZG_NC32_REASON_NONE ||
        result.stage != ZG_NC32_STAGE_DONE) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        return le4_bad_w(f);
    }
    return le4_finish(f, result.return_value32);
}

static zg_w154_state le4_length(void *c, zg_re4_raw_length_request raw)
{
    le4_frame *f = (le4_frame *)c;
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    if (raw.callPC32 != ZG_RE4_RAW_LENGTH_CALL_PC32 ||
        raw.continuation32 != ZG_RE4_RAW_LENGTH_CONTINUATION32 ||
        raw.encoded_message_literal32 != ZG_RE4_RAW_MESSAGE_LITERAL32 ||
        raw.entry.ecx32 != UINT32_C(0) ||
        raw.entry.eax32 != ZG_RE4_RAW_COUNT_LIMIT32) {
        zg116la_bridge_latch(&f->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        return le4_w_state(f);
    }
    request.version32 = ZG116LA_VERSION;
    request.seam32 = ZG116LA_SEAM_LENGTH_ERROR;
    request.ordinal32 = UINT32_C(2);
    request.target32 = raw.target32;
    request.call_pc32 = raw.callPC32;
    request.continuation_present = UINT32_C(1);
    request.continuation32 = raw.continuation32;
    request.message32 = raw.encoded_message_literal32;
    request.entry_present = UINT32_C(1);
    request.entry = le4_from_w(raw.entry);
    request.xmm0_entry = f->bridge.xmm0;
    (void)zg116la_invoke_machine(&f->bridge, request, &reply);
    /* RETURNED remains the frozen raw-frame unsupported frontier. */
    return le4_w_state(f);
}

void zg116la_e4_range(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply)
{
    le4_frame f = {0};
    zg_re4_raw_services ops;
    zg_re4_raw_result result;
    if (reply == 0) { return; }
    zg116la_lower_clear(reply);
    if (!zg116la_request_matches(&request, &le4_identity)) {
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
    ops.wrapper.context = &f;
    ops.wrapper.read32 = le4_wread;
    ops.wrapper.write32 = le4_wwrite;
    ops.wrapper.write8 = le4_wbyte;
    ops.wrapper.read_fs_head32 = le4_fsread;
    ops.wrapper.write_fs_head32 = le4_fswrite;
    ops.wrapper.call_first = le4_first;
    ops.wrapper.call_second = le4_second;
    ops.length_context = &f;
    ops.length_call = le4_length;
    result = zg_range101e0_raw_frame_normal(&ops, le4_to_w(request.entry));
    if (result.state != ZG_W154_RETURNED) {
        if (zg116la_bridge_status(&f->bridge) == ZG116_SERVICE_OK) {
            uint32_t state = ZG116_SERVICE_INVALID;
            if (result.state == ZG_W154_NONRETURN) { state = ZG116_SERVICE_STOPPED; }
            else if (result.state == ZG_W154_UNKNOWN ||
                result.reason == ZG_RE4_RAW_REASON_CD40_CONTROL_FRONTIER ||
                result.reason == ZG_RE4_RAW_REASON_LENGTH_RETURN_FRONTIER) {
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
    if (result.reason != ZG_RE4_RAW_REASON_NONE ||
        result.phase != ZG_RE4_RAW_PHASE_DONE ||
        result.final_return_valid != UINT32_C(1) ||
        result.return_cleanup32 != UINT32_C(16) ||
        result.return_value32 != result.registers32.eax32) {
        zg116la_bridge_latch(&f.bridge, ZG116_SERVICE_UNKNOWN,
            ZG116LA_REASON_CHILD_RESULT);
        zg116la_lower_from_bridge(&f.bridge, reply);
        return;
    }
    f.bridge.regs = le4_from_w(result.registers32);
    zg116la_lower_returned(reply, f.bridge.regs, f.bridge.xmm0,
        result.return_target32, result.return_cleanup32);
}
