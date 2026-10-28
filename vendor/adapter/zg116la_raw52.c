#include "zg116la_raw52.h"

#define ZG116LA52_OPERATOR_NEW_THUNK UINT32_C(0x101bca28)
#define ZG116LA52_FAILURE_IAT UINT32_C(0x101d7274)

typedef struct zg116la52_frame {
    zg116la_bridge bridge;
    uint32_t target32;
    uint32_t first_seen;
    uint32_t first32;
    uint32_t second_seen;
    uint32_t second32;
    uint32_t writes_completed32;
} zg116la52_frame;

static const zg116la_identity zg116la52_fe0 = {
    UINT32_C(0x10007fe0), UINT32_C(1), ZG116_ROLE_SENTINEL,
    UINT32_C(0x10008017), UINT32_C(0x1000a130), UINT32_C(0x1000801c)
};
static const zg116la_identity zg116la52_e4c0 = {
    UINT32_C(0x1001e4c0), UINT32_C(1), ZG116_ROLE_SENTINEL,
    UINT32_C(0x1001e4f7), UINT32_C(0x1002ee30), UINT32_C(0x1001e4fc)
};

static uint32_t zg116la52_add(uint32_t a, uint32_t b)
{
    return (uint32_t)((unsigned long long)a + (unsigned long long)b);
}

static zg_np52_state zg116la52_child_state(zg116la52_frame *frame)
{
    uint32_t state32 = zg116la_bridge_status(&frame->bridge);
    if (state32 == ZG116_SERVICE_STOPPED) {
        return ZG_NP52_NONRETURN;
    }
    if (state32 == ZG116_SERVICE_OK) {
        return ZG_NP52_RETURNED;
    }
    /* UNKNOWN is deliberately projected as ERROR to this three-state child;
     * the outer bridge retains UNKNOWN and is never overwritten by the child.
     */
    return ZG_NP52_ERROR;
}

static int zg116la52_push(zg116la52_frame *frame, uint32_t word32)
{
    frame->bridge.regs.esp = zg116la52_add(
        frame->bridge.regs.esp, UINT32_C(0xfffffffc));
    return zg116la_write32(&frame->bridge, ZG116_SPACE_RAM,
        frame->bridge.regs.esp, word32) == ZG116_SERVICE_OK;
}

static zg116la_machine_call zg116la52_call(zg116la52_frame *frame,
    uint32_t seam32, uint32_t target32, uint32_t call_pc32,
    uint32_t continuation32, uint32_t request_bytes32)
{
    zg116la_machine_call request = {0};
    request.version32 = ZG116LA_VERSION;
    request.seam32 = seam32;
    request.ordinal32 = UINT32_C(1);
    request.target32 = target32;
    request.call_pc32 = call_pc32;
    request.continuation_present = UINT32_C(1);
    request.continuation32 = continuation32;
    request.request_bytes32 = request_bytes32;
    request.callee_ret_cleanup32 = UINT32_C(0);
    request.entry_present = UINT32_C(1);
    request.entry = frame->bridge.regs;
    request.xmm0_entry = frame->bridge.xmm0;
    return request;
}

static zg_np52_call_result zg116la52_project(zg116la52_frame *frame,
    zg116la_machine_call request, zg116la_machine_reply reply,
    uint32_t state32)
{
    zg_np52_call_result result = {0};
    uint32_t expected_esp;
    result.state = zg116la52_child_state(frame);
    if (state32 != ZG116_SERVICE_OK) {
        return result;
    }
    /* Both imports have no callee-cleaned ordinary arguments. Their actual
     * post-RET ESP is callee-entry ESP+4; the caller ADD belongs only to new.
     */
    expected_esp = zg116la52_add(request.entry.esp, UINT32_C(4));
    if (reply.returned.esp != expected_esp) {
        zg116la_bridge_latch(&frame->bridge, ZG116_SERVICE_UNKNOWN,
            ZG116LA_REASON_MACHINE_REPLY);
        result.state = ZG_NP52_ERROR;
        return result;
    }
    result.state = ZG_NP52_RETURNED;
    result.eax32 = reply.returned.eax;
    result.normal_abi_preserved = UINT32_C(1);
    return result;
}

static zg_np52_call_result zg116la52_allocate(void *context, uint32_t bytes32)
{
    zg116la52_frame *frame = (zg116la52_frame *)context;
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    uint32_t state32;
    if (bytes32 != UINT32_C(36) ||
        !zg116la52_push(frame, bytes32) ||
        !zg116la52_push(frame, zg116la52_add(frame->target32, UINT32_C(7)))) {
        zg116la_bridge_latch(&frame->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return zg116la52_project(frame, request, reply,
            ZG116_SERVICE_INVALID);
    }
    request = zg116la52_call(frame, ZG116LA_SEAM_ALLOCATE,
        ZG116LA52_OPERATOR_NEW_THUNK,
        zg116la52_add(frame->target32, UINT32_C(2)),
        zg116la52_add(frame->target32, UINT32_C(7)), bytes32);
    state32 = zg116la_invoke_machine(&frame->bridge, request, &reply);
    if (state32 == ZG116_SERVICE_OK) {
        frame->bridge.regs.esp = zg116la52_add(
            frame->bridge.regs.esp, UINT32_C(4));
    }
    return zg116la52_project(frame, request, reply, state32);
}

static zg_np52_call_result zg116la52_failure(void *context)
{
    zg116la52_frame *frame = (zg116la52_frame *)context;
    zg116la_machine_call request = {0};
    zg116la_machine_reply reply = {0};
    uint32_t target32 = UINT32_C(0);
    uint32_t state32;
    if (zg116la_read32(&frame->bridge, ZG116_SPACE_RAM,
            ZG116LA52_FAILURE_IAT, &target32) != ZG116_SERVICE_OK ||
        !zg116la52_push(frame, zg116la52_add(frame->target32, UINT32_C(20)))) {
        return zg116la52_project(frame, request, reply,
            zg116la_bridge_status(&frame->bridge));
    }
    request = zg116la52_call(frame, ZG116LA_SEAM_FAILURE_IMPORT,
        target32, zg116la52_add(frame->target32, UINT32_C(14)),
        zg116la52_add(frame->target32, UINT32_C(20)), UINT32_C(0));
    state32 = zg116la_invoke_machine(&frame->bridge, request, &reply);
    return zg116la52_project(frame, request, reply, state32);
}

static zg_np52_state zg116la52_argument(void *context,
    uint32_t argument_index, uint32_t *word_out)
{
    zg116la52_frame *frame = (zg116la52_frame *)context;
    uint32_t address32;
    if ((argument_index != UINT32_C(1) && argument_index != UINT32_C(2)) ||
        word_out == 0) {
        zg116la_bridge_latch(&frame->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return zg116la52_child_state(frame);
    }
    /* Fresh [actual current ESP+4/+8], never a descriptor-derived slot. */
    address32 = zg116la52_add(frame->bridge.regs.esp,
        argument_index * UINT32_C(4));
    if (zg116la_read32(&frame->bridge, ZG116_SPACE_RAM,
            address32, word_out) != ZG116_SERVICE_OK) {
        return zg116la52_child_state(frame);
    }
    if (argument_index == UINT32_C(1)) {
        frame->first_seen = UINT32_C(1);
        frame->first32 = *word_out;
        frame->bridge.regs.ecx = *word_out;
    } else {
        frame->second_seen = UINT32_C(1);
        frame->second32 = *word_out;
        frame->bridge.regs.edx = *word_out;
    }
    return ZG_NP52_RETURNED;
}

static zg_np52_state zg116la52_write(void *context,
    uint32_t address32, uint32_t word32)
{
    zg116la52_frame *frame = (zg116la52_frame *)context;
    uint32_t expected32;
    if (frame->writes_completed32 == UINT32_C(0)) {
        expected32 = frame->bridge.regs.eax;
    } else if (frame->writes_completed32 == UINT32_C(1)) {
        expected32 = zg116la52_add(frame->bridge.regs.eax, UINT32_C(4));
        /* LEA ECX,[EAX+4] precedes the wrapped-zero guard and second MOV. */
        frame->bridge.regs.ecx = expected32;
    } else {
        zg116la_bridge_latch(&frame->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        return zg116la52_child_state(frame);
    }
    if (address32 != expected32) {
        zg116la_bridge_latch(&frame->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        return zg116la52_child_state(frame);
    }
    if (zg116la_write32(&frame->bridge, ZG116_SPACE_RAM,
            address32, word32) != ZG116_SERVICE_OK) {
        return zg116la52_child_state(frame);
    }
    frame->writes_completed32 = zg116la52_add(
        frame->writes_completed32, UINT32_C(1));
    return ZG_NP52_RETURNED;
}

static void zg116la52_finish(zg116la52_frame *frame,
    zg_np52_result core, zg116_lower_reply *reply)
{
    uint32_t second32;
    uint32_t second_address32;
    uint32_t expected_writes32;
    uint32_t return_target32 = UINT32_C(0);
    if (core.state != ZG_NP52_RETURNED) {
        if (zg116la_bridge_status(&frame->bridge) == ZG116_SERVICE_OK) {
            zg116la_bridge_latch(&frame->bridge,
                core.state == ZG_NP52_NONRETURN ? ZG116_SERVICE_STOPPED :
                    ZG116_SERVICE_INVALID, ZG116LA_REASON_CHILD_RESULT);
        }
        zg116la_lower_from_bridge(&frame->bridge, reply);
        return;
    }
    if (zg116la_bridge_status(&frame->bridge) != ZG116_SERVICE_OK) {
        zg116la_lower_from_bridge(&frame->bridge, reply);
        return;
    }
    second_address32 = zg116la52_add(core.return_value32, UINT32_C(4));
    expected_writes32 = second_address32 == UINT32_C(0) ?
        UINT32_C(1) : UINT32_C(2);
    if (core.reason != ZG_NP52_REASON_NONE ||
        core.stage != ZG_NP52_STAGE_DONE || frame->first_seen != UINT32_C(1) ||
        (frame->first32 != UINT32_C(0) && frame->second_seen != UINT32_C(1)) ||
        frame->writes_completed32 != expected_writes32) {
        zg116la_bridge_latch(&frame->bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_CHILD_RESULT);
        zg116la_lower_from_bridge(&frame->bridge, reply);
        return;
    }
    second32 = frame->first32 == UINT32_C(0) ?
        core.return_value32 : frame->second32;
    frame->bridge.regs.eax = core.return_value32;
    frame->bridge.regs.edx = second32;
    frame->bridge.regs.ecx = second_address32;
    if (zg116la_read32(&frame->bridge, ZG116_SPACE_RAM,
            frame->bridge.regs.esp, &return_target32) != ZG116_SERVICE_OK) {
        zg116la_lower_from_bridge(&frame->bridge, reply);
        return;
    }
    frame->bridge.regs.esp = zg116la52_add(
        frame->bridge.regs.esp, UINT32_C(12));
    zg116la_lower_returned(reply, frame->bridge.regs, frame->bridge.xmm0,
        return_target32, UINT32_C(8));
}

static void zg116la52_run(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply,
    const zg116la_identity *identity)
{
    zg116la52_frame frame = {0};
    zg_np52_services services;
    zg_np52_result core;
    if (reply == 0) {
        return;
    }
    zg116la_lower_clear(reply);
    if (!zg116la_request_matches(&request, identity)) {
        reply->state32 = ZG116_LOWER_INVALID;
        reply->reason32 = ZG116LA_REASON_REQUEST_IDENTITY;
        return;
    }
    if (!zg116la_bridge_begin(&frame.bridge,
            (const zg116la_config *)user, memory, &request)) {
        reply->state32 = ZG116_LOWER_INVALID;
        reply->reason32 = ZG116LA_REASON_BAD_INTERFACE;
        return;
    }
    frame.target32 = identity->target32;
    services.context = &frame;
    services.allocate = zg116la52_allocate;
    services.failure_import = zg116la52_failure;
    services.read_argument32 = zg116la52_argument;
    services.write32 = zg116la52_write;
    core = zg_np52_provider_candidate(&services);
    zg116la52_finish(&frame, core, reply);
}

void zg116la_fe0_sentinel(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply)
{
    zg116la52_run(user, request, memory, reply, &zg116la52_fe0);
}

void zg116la_e4c0_sentinel(void *user, zg116_call request,
    const zg116_memory *memory, zg116_lower_reply *reply)
{
    zg116la52_run(user, request, memory, reply, &zg116la52_e4c0);
}
