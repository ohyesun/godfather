/* Typed C11 composition SOURCE candidate. Neither frozen core is modified.
 * The four inner bodies stay supplied typed opaque providers. This adds no
 * guest operation, continuation push, observer ABI, allocator or global state.
 */
#include "zg_copy335_rich116.h"

/* Memory-reply words are deliberately forwarded without normalization. */
_Static_assert(ZG335_SPACE_RAM == ZG116_SPACE_RAM, "same logical RAM domain");
_Static_assert(ZG335_SPACE_LOGICAL_FS == ZG116_SPACE_LOGICAL_FS,
    "same explicitly selected logical FS domain");
_Static_assert(ZG335_SERVICE_UNSET == ZG116_SERVICE_UNSET, "same service0");
_Static_assert(ZG335_SERVICE_OK == ZG116_SERVICE_OK, "same service1");
_Static_assert(ZG335_SERVICE_STOPPED == ZG116_SERVICE_STOPPED, "same service2");
_Static_assert(ZG335_SERVICE_INVALID == ZG116_SERVICE_INVALID, "same service3");
_Static_assert(ZG335_SERVICE_UNKNOWN == ZG116_SERVICE_UNKNOWN, "same service4");

typedef struct zg335116_context {
    zg335116_config configuration;
    zg335116_output *output;
} zg335116_context;

typedef struct zg335116_memory_bridge {
    zg335_memory memory;
} zg335116_memory_bridge;

static zg116_regs8 zg335116_to_normal_regs(zg335_regs8 source)
{
    zg116_regs8 destination = {0};
    destination.eax = source.eax;
    destination.ecx = source.ecx;
    destination.edx = source.edx;
    destination.ebx = source.ebx;
    destination.esp = source.esp;
    destination.ebp = source.ebp;
    destination.esi = source.esi;
    destination.edi = source.edi;
    return destination;
}

static zg335_regs8 zg335116_to_raw_regs(zg116_regs8 source)
{
    zg335_regs8 destination = {0};
    destination.eax = source.eax;
    destination.ecx = source.ecx;
    destination.edx = source.edx;
    destination.ebx = source.ebx;
    destination.esp = source.esp;
    destination.ebp = source.ebp;
    destination.esi = source.esi;
    destination.edi = source.edi;
    return destination;
}

static zg116_xmm0 zg335116_to_normal_xmm(zg335_xmm0 source)
{
    zg116_xmm0 destination = {0};
    destination.w0 = source.w0;
    destination.w1 = source.w1;
    destination.w2 = source.w2;
    destination.w3 = source.w3;
    return destination;
}

static zg335_xmm0 zg335116_to_raw_xmm(zg116_xmm0 source)
{
    zg335_xmm0 destination = {0};
    destination.w0 = source.w0;
    destination.w1 = source.w1;
    destination.w2 = source.w2;
    destination.w3 = source.w3;
    return destination;
}

static void zg335116_read(void *user, uint32_t space32,
    uint32_t address32, uint32_t width32, zg116_read_reply *reply)
{
    zg335116_memory_bridge *bridge = user;
    zg335_read_reply raw_reply = {0};
    uint32_t i;
    bridge->memory.read(bridge->memory.user, space32, address32,
        width32, &raw_reply);
    reply->state32 = raw_reply.state32;
    reply->reason32 = raw_reply.reason32;
    reply->initialized_mask32 = raw_reply.initialized_mask32;
    for (i = UINT32_C(0); i < UINT32_C(16); ++i) {
        reply->data.byte[i] = raw_reply.data.byte[i];
    }
}

static void zg335116_write(void *user, uint32_t space32,
    uint32_t address32, uint32_t width32, zg116_block16 data,
    zg116_write_reply *reply)
{
    zg335116_memory_bridge *bridge = user;
    zg335_block16 raw_data = {0};
    zg335_write_reply raw_reply = {0};
    uint32_t i;
    for (i = UINT32_C(0); i < UINT32_C(16); ++i) {
        raw_data.byte[i] = data.byte[i];
    }
    bridge->memory.write(bridge->memory.user, space32, address32,
        width32, raw_data, &raw_reply);
    reply->state32 = raw_reply.state32;
    reply->reason32 = raw_reply.reason32;
    reply->committed_prefix32 = raw_reply.committed_prefix32;
}

/* Explicit policy compression, not an enum cast or original CPU statement.
 * An unnamed numeric normal state remains verbatim in normal_result and is an
 * invariant-breach record; the raw endpoint still fails closed as INVALID.
 */
static uint32_t zg335116_lower_state(uint32_t normal_state)
{
    switch (normal_state) {
    case ZG116_MEMORY_STOPPED:
    case ZG116_CALLEE_STOPPED:
        return ZG335_LOWER_STOPPED;
    case ZG116_MEMORY_UNKNOWN:
    case ZG116_CALLEE_UNKNOWN:
        return ZG335_LOWER_UNKNOWN;
    case ZG116_RETURNED:
        return ZG335_LOWER_RETURNED;
    case ZG116_RESULT_UNSET:
    case ZG116_BAD_INTERFACE:
    case ZG116_BAD_ENTRY:
    case ZG116_BAD_WRAPPER:
    case ZG116_MEMORY_INVALID:
    case ZG116_MEMORY_REPLY_INVALID:
    case ZG116_CALLEE_INVALID:
    case ZG116_CALLEE_REPLY_INVALID:
    case ZG116_RETURN_CONTROL_INVALID:
    default:
        return ZG335_LOWER_INVALID;
    }
}

static zg335_lower_reply zg335116_project(zg116_result result)
{
    zg335_lower_reply reply = {0};
    reply.state32 = zg335116_lower_state(result.state32);
    reply.reason32 = result.reason32;
    reply.regs_present = result.regs_present;
    reply.returned = zg335116_to_raw_regs(result.regs);
    reply.xmm0_present = result.xmm0_present;
    reply.xmm0_returned = zg335116_to_raw_xmm(result.xmm0);
    reply.returned_to_present = result.final_target_present;
    reply.returned_to32 = result.final_target32;
    reply.cleanup_present = result.final_cleanup_present;
    reply.ret_cleanup32 = result.final_cleanup32;
    return reply;
}

/* Private adapter domain: request/memory/reply come from the frozen raw core.
 * The actual target word selects the rich entry; family only selects the
 * already-captured provider pair for this correctly typed raw hook.
 */
static void zg335116_lower(zg335116_context *context, zg335_call request,
    const zg335_memory *memory, zg335_lower_reply *reply, uint32_t family)
{
    zg335116_memory_bridge bridge;
    zg116_services services = {0};
    zg116_entry entry = {0};
    zg116_result result;
    uint32_t slot;

    bridge.memory = *memory;
    services.version32 = ZG116_VERSION;
    services.memory.user = &bridge;
    services.memory.read = zg335116_read;
    services.memory.write = zg335116_write;
    if (family == ZG116_ENTRY_E4C0) {
        services.sentinel_user = context->configuration.e4c0_sentinel_user;
        services.sentinel = context->configuration.e4c0_sentinel;
        services.range_user = context->configuration.e4c0_range_user;
        services.range = context->configuration.e4c0_range;
    } else {
        services.sentinel_user = context->configuration.fe0_sentinel_user;
        services.sentinel = context->configuration.fe0_sentinel;
        services.range_user = context->configuration.fe0_range_user;
        services.range = context->configuration.fe0_range;
    }
    entry.wrapper_entry32 = request.target32;
    entry.regs_present = UINT32_C(1);
    entry.regs = zg335116_to_normal_regs(request.entry);
    entry.xmm0_present = UINT32_C(1);
    entry.xmm0 = zg335116_to_normal_xmm(request.xmm0_entry);

    /* No CALL push, validation read, restoration or forced ESP at this layer. */
    result = zg116_normal_frame(entry, &services);
    *reply = zg335116_project(result);

    /* Overflow can never suppress or alter that actual call/projection. The
     * frozen raw core has at most8 dispatches. This is a defensive9th boundary.
     */
    slot = context->output->normal_count32;
    if (slot < ZG335116_NORMAL_RECORD_SLOTS) {
        context->output->normal_records[slot].request = request;
        context->output->normal_records[slot].normal_result = result;
        context->output->normal_records[slot].projected_reply = *reply;
        context->output->normal_count32 = slot + UINT32_C(1);
    } else {
        context->output->record_overflow32 = UINT32_C(1);
    }
}

static void zg335116_e4c0(void *user, zg335_call request,
    const zg335_memory *memory, zg335_lower_reply *reply)
{
    zg335116_context *context = user;
    zg335116_lower(context, request, memory, reply, ZG116_ENTRY_E4C0);
}

static void zg335116_fe0(void *user, zg335_call request,
    const zg335_memory *memory, zg335_lower_reply *reply)
{
    zg335116_context *context = user;
    zg335116_lower(context, request, memory, reply, ZG116_ENTRY_7FE0);
}

uint32_t zg335116_run(zg335_entry entry,
    const zg335116_config *configuration, zg335116_output *output)
{
    zg335116_context context;
    zg335_services services = {0};

    if (output == 0) {
        return ZG335116_HOST_BAD_INTERFACE;
    }
    *output = (zg335116_output){0};
    if (configuration == 0) {
        return ZG335116_HOST_BAD_INTERFACE;
    }
    context.configuration = *configuration;
    context.output = output;
    if (context.configuration.version32 != ZG335116_VERSION
        || context.configuration.memory.read == 0
        || context.configuration.memory.write == 0
        || context.configuration.e4c0_sentinel == 0
        || context.configuration.e4c0_range == 0
        || context.configuration.fe0_sentinel == 0
        || context.configuration.fe0_range == 0) {
        return ZG335116_HOST_BAD_INTERFACE;
    }
    services.version32 = ZG335_VERSION;
    services.memory = context.configuration.memory;
    services.e4c0_user = &context;
    services.e4c0 = zg335116_e4c0;
    services.fe0_user = &context;
    services.fe0 = zg335116_fe0;
    output->raw_result = zg335_raw_normal_frame(entry, &services);
    output->raw_result_present = UINT32_C(1);
    if (output->record_overflow32 != UINT32_C(0)) {
        return ZG335116_HOST_CAPTURE_INCOMPLETE;
    }
    return ZG335116_HOST_COMPLETED;
}
