#include "zg116la_common.h"

static int zg116la_nonreturn(uint32_t state32)
{
    return state32 == ZG116_SERVICE_STOPPED ||
        state32 == ZG116_SERVICE_INVALID ||
        state32 == ZG116_SERVICE_UNKNOWN;
}

static uint32_t zg116la_word(const zg116_block16 *data)
{
    return (uint32_t)data->byte[0] |
        (uint32_t)data->byte[1] << 8 |
        (uint32_t)data->byte[2] << 16 |
        (uint32_t)data->byte[3] << 24;
}

void zg116la_lower_clear(zg116_lower_reply *reply)
{
    zg116_lower_reply zero = {0};
    if (reply != 0) {
        *reply = zero;
    }
}

int zg116la_request_matches(const zg116_call *request,
    const zg116la_identity *identity)
{
    if (request == 0 || identity == 0) {
        return 0;
    }
    return request->wrapper_entry32 == identity->wrapper_entry32 &&
        request->ordinal32 == identity->ordinal32 &&
        request->role32 == identity->role32 &&
        request->call_pc32 == identity->call_pc32 &&
        request->target32 == identity->target32 &&
        request->continuation32 == identity->continuation32;
}

int zg116la_bridge_begin(zg116la_bridge *bridge,
    const zg116la_config *config, const zg116_memory *memory,
    const zg116_call *request)
{
    zg116la_bridge zero = {0};
    if (bridge == 0) {
        return 0;
    }
    *bridge = zero;
    if (config == 0 || memory == 0 || request == 0 ||
        config->version32 != ZG116LA_VERSION ||
        config->machine_call == 0 || memory->read == 0 ||
        memory->write == 0) {
        return 0;
    }
    bridge->config = *config;
    bridge->memory = *memory;
    bridge->regs = request->entry;
    bridge->xmm0 = request->xmm0_entry;
    return 1;
}

uint32_t zg116la_bridge_status(const zg116la_bridge *bridge)
{
    if (bridge == 0) {
        return ZG116_SERVICE_INVALID;
    }
    if (zg116la_nonreturn(bridge->stopped_state32)) {
        return bridge->stopped_state32;
    }
    return ZG116_SERVICE_OK;
}

void zg116la_bridge_latch(zg116la_bridge *bridge,
    uint32_t state32, uint32_t reason32)
{
    if (bridge == 0) {
        return;
    }
    if (!zg116la_nonreturn(state32)) {
        state32 = ZG116_SERVICE_INVALID;
        reason32 = ZG116LA_REASON_BAD_INTERFACE;
    }
    if (!zg116la_nonreturn(bridge->stopped_state32)) {
        bridge->stopped_state32 = state32;
        bridge->reason32 = reason32;
    }
}

void zg116la_lower_from_bridge(const zg116la_bridge *bridge,
    zg116_lower_reply *reply)
{
    uint32_t state32 = zg116la_bridge_status(bridge);
    if (reply == 0) {
        return;
    }
    zg116la_lower_clear(reply);
    if (state32 == ZG116_SERVICE_STOPPED) {
        reply->state32 = ZG116_LOWER_STOPPED;
    } else if (state32 == ZG116_SERVICE_UNKNOWN) {
        reply->state32 = ZG116_LOWER_UNKNOWN;
    } else {
        reply->state32 = ZG116_LOWER_INVALID;
    }
    reply->reason32 = bridge == 0 ? ZG116LA_REASON_BAD_INTERFACE :
        bridge->reason32;
}

void zg116la_lower_returned(zg116_lower_reply *reply,
    zg116_regs8 regs, zg116_xmm0 xmm0,
    uint32_t returned_to32, uint32_t ret_cleanup32)
{
    if (reply == 0) {
        return;
    }
    zg116la_lower_clear(reply);
    reply->state32 = ZG116_LOWER_RETURNED;
    reply->reason32 = ZG116LA_REASON_NONE;
    reply->regs_present = UINT32_C(1);
    reply->returned = regs;
    reply->xmm0_present = UINT32_C(1);
    reply->xmm0_returned = xmm0;
    reply->returned_to_present = UINT32_C(1);
    reply->returned_to32 = returned_to32;
    reply->cleanup_present = UINT32_C(1);
    reply->ret_cleanup32 = ret_cleanup32;
}

static uint32_t zg116la_read(zg116la_bridge *bridge, uint32_t space32,
    uint32_t address32, uint32_t width32, zg116_read_reply *reply)
{
    zg116_read_reply zero = {0};
    if (bridge == 0 || reply == 0 ||
        zg116la_bridge_status(bridge) != ZG116_SERVICE_OK) {
        return ZG116_SERVICE_INVALID;
    }
    *reply = zero;
    if ((space32 != ZG116_SPACE_RAM && space32 != ZG116_SPACE_LOGICAL_FS) ||
        (width32 != UINT32_C(1) && width32 != UINT32_C(4))) {
        zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return bridge->stopped_state32;
    }
    bridge->memory.read(bridge->memory.user, space32, address32,
        width32, reply);
    if (reply->state32 == ZG116_SERVICE_OK) {
        return ZG116_SERVICE_OK;
    }
    if (zg116la_nonreturn(reply->state32)) {
        zg116la_bridge_latch(bridge, reply->state32, reply->reason32);
        return bridge->stopped_state32;
    }
    zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
        ZG116LA_REASON_READ_REPLY);
    return bridge->stopped_state32;
}

uint32_t zg116la_read32(zg116la_bridge *bridge, uint32_t space32,
    uint32_t address32, uint32_t *word_out)
{
    zg116_read_reply reply;
    uint32_t state32;
    uint32_t i;
    if (word_out == 0) {
        zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return ZG116_SERVICE_INVALID;
    }
    state32 = zg116la_read(bridge, space32, address32, UINT32_C(4), &reply);
    if (state32 != ZG116_SERVICE_OK) {
        return state32;
    }
    if (reply.initialized_mask32 != UINT32_C(15)) {
        zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_READ_REPLY);
        return bridge->stopped_state32;
    }
    for (i = UINT32_C(4); i < UINT32_C(16); ++i) {
        if (reply.data.byte[i] != UINT8_C(0)) {
            zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
                ZG116LA_REASON_READ_REPLY);
            return bridge->stopped_state32;
        }
    }
    *word_out = zg116la_word(&reply.data);
    return ZG116_SERVICE_OK;
}

static uint32_t zg116la_write(zg116la_bridge *bridge, uint32_t space32,
    uint32_t address32, uint32_t width32, zg116_block16 data)
{
    zg116_write_reply reply = {0};
    if (bridge == 0 || zg116la_bridge_status(bridge) != ZG116_SERVICE_OK) {
        return ZG116_SERVICE_INVALID;
    }
    if ((space32 != ZG116_SPACE_RAM && space32 != ZG116_SPACE_LOGICAL_FS) ||
        (width32 != UINT32_C(1) && width32 != UINT32_C(4))) {
        zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return bridge->stopped_state32;
    }
    bridge->memory.write(bridge->memory.user, space32, address32,
        width32, data, &reply);
    if (reply.state32 == ZG116_SERVICE_OK) {
        if (reply.committed_prefix32 != width32) {
            zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
                ZG116LA_REASON_WRITE_REPLY);
            return bridge->stopped_state32;
        }
        return ZG116_SERVICE_OK;
    }
    if (zg116la_nonreturn(reply.state32)) {
        zg116la_bridge_latch(bridge, reply.state32, reply.reason32);
        return bridge->stopped_state32;
    }
    zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
        ZG116LA_REASON_WRITE_REPLY);
    return bridge->stopped_state32;
}

uint32_t zg116la_write32(zg116la_bridge *bridge, uint32_t space32,
    uint32_t address32, uint32_t word32)
{
    zg116_block16 data = {{0}};
    uint32_t i;
    for (i = UINT32_C(0); i < UINT32_C(4); ++i) {
        data.byte[i] = (uint8_t)(word32 >> (i * UINT32_C(8)));
    }
    return zg116la_write(bridge, space32, address32, UINT32_C(4), data);
}

uint32_t zg116la_write8(zg116la_bridge *bridge, uint32_t space32,
    uint32_t address32, uint32_t low_byte)
{
    zg116_block16 data = {{0}};
    data.byte[0] = (uint8_t)low_byte;
    return zg116la_write(bridge, space32, address32, UINT32_C(1), data);
}

uint32_t zg116la_invoke_machine(zg116la_bridge *bridge,
    zg116la_machine_call request, zg116la_machine_reply *reply_out)
{
    zg116la_machine_reply reply = {0};
    if (reply_out == 0) {
        zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return ZG116_SERVICE_INVALID;
    }
    *reply_out = reply;
    if (bridge == 0 || zg116la_bridge_status(bridge) != ZG116_SERVICE_OK) {
        return ZG116_SERVICE_INVALID;
    }
    if (request.version32 != ZG116LA_VERSION ||
        (request.continuation_present != UINT32_C(0) &&
            request.continuation_present != UINT32_C(1)) ||
        (request.entry_present != UINT32_C(0) &&
            request.entry_present != UINT32_C(1)) ||
        (request.continuation_present == UINT32_C(1) &&
            request.entry_present != UINT32_C(1))) {
        zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
            ZG116LA_REASON_BAD_INTERFACE);
        return bridge->stopped_state32;
    }
    bridge->config.machine_call(bridge->config.machine_user, request, &reply);
    *reply_out = reply;
    if (reply.state32 != ZG116_SERVICE_OK) {
        if (zg116la_nonreturn(reply.state32)) {
            zg116la_bridge_latch(bridge, reply.state32, reply.reason32);
        } else {
            zg116la_bridge_latch(bridge, ZG116_SERVICE_INVALID,
                ZG116LA_REASON_MACHINE_REPLY);
        }
        return bridge->stopped_state32;
    }
    if (reply.normal_abi_preserved != UINT32_C(1) ||
        reply.regs_present != UINT32_C(1) ||
        reply.xmm0_present != UINT32_C(1) ||
        reply.returned_to_present != UINT32_C(1) ||
        (request.continuation_present == UINT32_C(1) &&
            reply.returned_to32 != request.continuation32)) {
        zg116la_bridge_latch(bridge, ZG116_SERVICE_UNKNOWN,
            ZG116LA_REASON_MACHINE_REPLY);
        return bridge->stopped_state32;
    }
    bridge->regs = reply.returned;
    bridge->xmm0 = reply.xmm0_returned;
    return ZG116_SERVICE_OK;
}
