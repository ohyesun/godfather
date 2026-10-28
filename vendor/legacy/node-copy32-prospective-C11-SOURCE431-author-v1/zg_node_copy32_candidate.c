#include "zg_node_copy32_candidate.h"

typedef struct zg_nc32_frame {
    zg_nc32_services ops;
    zg_nc32_reason reason;
    zg_nc32_stage stage;
} zg_nc32_frame;

static uint32_t zg_nc32_add(uint32_t a, uint32_t b)
{
    return (uint32_t)(a + b);
}

static zg_nc32_state zg_nc32_state_checked(
    zg_nc32_frame *f, zg_nc32_state state)
{
    if (state == ZG_NC32_RETURNED) {
        return state;
    }
    if (state == ZG_NC32_NONRETURN || state == ZG_NC32_ERROR) {
        f->reason = ZG_NC32_REASON_SERVICE_STOP;
        return state;
    }
    f->reason = ZG_NC32_REASON_INVALID_STATE;
    return ZG_NC32_ERROR;
}

static zg_nc32_state zg_nc32_read(
    zg_nc32_frame *f, zg_nc32_stage stage, uint32_t a, uint32_t *out)
{
    f->stage = stage;
    return zg_nc32_state_checked(f, f->ops.read32(f->ops.context, a, out));
}

static zg_nc32_state zg_nc32_write(
    zg_nc32_frame *f, zg_nc32_stage stage, uint32_t a, uint32_t value)
{
    f->stage = stage;
    return zg_nc32_state_checked(f, f->ops.write32(f->ops.context, a, value));
}

static zg_nc32_state zg_nc32_xadd(
    zg_nc32_frame *f, zg_nc32_stage stage, uint32_t a,
    uint32_t delta, uint32_t *old)
{
    f->stage = stage;
    /* The injected primitive owns the entire locked32 RMW. No read/write split. */
    return zg_nc32_state_checked(
        f, f->ops.atomic_xadd32(f->ops.context, a, delta, old));
}

static zg_nc32_state zg_nc32_call(
    zg_nc32_frame *f, uint32_t object32, uint32_t slot,
    zg_nc32_stage table_stage, zg_nc32_stage target_stage,
    zg_nc32_stage call_stage, zg_nc32_slot_fn invoke)
{
    uint32_t table32;
    uint32_t target32;
    zg_nc32_state state;
    zg_nc32_opaque_result reply;
    state = zg_nc32_read(f, table_stage, object32, &table32);
    if (state != ZG_NC32_RETURNED) {
        return state;
    }
    state = zg_nc32_read(f, target_stage, zg_nc32_add(table32, slot), &target32);
    if (state != ZG_NC32_RETURNED) {
        return state;
    }
    f->stage = call_stage;
    reply = invoke(f->ops.context, object32, table32, target32);
    state = zg_nc32_state_checked(f, reply.state);
    if (state != ZG_NC32_RETURNED) {
        return state;
    }
    if (reply.normal_abi_preserved != UINT32_C(1)) {
        /* New portable stop policy. The original has no such runtime guard. */
        f->reason = ZG_NC32_REASON_ABI_UNCONFIRMED;
        return ZG_NC32_ERROR;
    }
    return ZG_NC32_RETURNED;
}

#define ZG_NC32_TRY(expression) \
    do { \
        state = (expression); \
        if (state != ZG_NC32_RETURNED) { \
            goto stopped; \
        } \
    } while (0)

zg_nc32_result zg_nc32_copy7_candidate(
    const zg_nc32_services *services,
    uint32_t destination32, uint32_t source32)
{
    zg_nc32_frame f;
    zg_nc32_result result;
    zg_nc32_state state;
    uint32_t word;
    uint32_t incoming_first;
    uint32_t compared_first;
    uint32_t old_first;
    uint32_t incoming_second;
    uint32_t captured_data;
    uint32_t old_second;
    uint32_t old_count;
    uint32_t sum;
    uint32_t decremented_returned_old;
    uint32_t payload16;

    result.state = ZG_NC32_ERROR;
    result.reason = ZG_NC32_REASON_BAD_INTERFACE;
    result.stage = ZG_NC32_STAGE_INTERFACE;
    result.destination32 = destination32;
    result.return_value32 = UINT32_C(0);
    if (services == 0) {
        return result;
    }
    f.ops = *services;
    if (f.ops.read32 == 0 || f.ops.write32 == 0 ||
        f.ops.atomic_xadd32 == 0 || f.ops.call_slot4 == 0 ||
        f.ops.call_slot8 == 0) {
        return result;
    }
    f.reason = ZG_NC32_REASON_NONE;
    f.stage = ZG_NC32_STAGE_INTERFACE;

    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_SOURCE0, source32, &word));
    ZG_NC32_TRY(zg_nc32_write(&f, ZG_NC32_STAGE_WRITE_DEST0, destination32, word));
    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_SOURCE4,
        zg_nc32_add(source32, UINT32_C(4)), &incoming_first));
    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_COMPARE_DEST4,
        zg_nc32_add(destination32, UINT32_C(4)), &compared_first));
    if (incoming_first != compared_first) {
        if (incoming_first != UINT32_C(0)) {
            ZG_NC32_TRY(zg_nc32_xadd(&f, ZG_NC32_STAGE_INC_INCOMING_WEAK,
                zg_nc32_add(incoming_first, UINT32_C(8)), UINT32_C(1), &old_count));
        }
        ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_OLD_DEST4,
            zg_nc32_add(destination32, UINT32_C(4)), &old_first));
        if (old_first != UINT32_C(0)) {
            ZG_NC32_TRY(zg_nc32_xadd(&f, ZG_NC32_STAGE_DEC_OLD_WEAK,
                zg_nc32_add(old_first, UINT32_C(8)), UINT32_C(0xffffffff), &old_count));
            sum = zg_nc32_add(old_count, UINT32_C(0xffffffff));
            if (sum == UINT32_C(0)) {
                ZG_NC32_TRY(zg_nc32_call(&f, old_first, UINT32_C(8),
                    ZG_NC32_STAGE_READ_FIRST_VTABLE, ZG_NC32_STAGE_READ_FIRST_SLOT8,
                    ZG_NC32_STAGE_CALL_FIRST_SLOT8, f.ops.call_slot8));
            }
        }
        ZG_NC32_TRY(zg_nc32_write(&f, ZG_NC32_STAGE_WRITE_DEST4,
            zg_nc32_add(destination32, UINT32_C(4)), incoming_first));
    }

    payload16 = zg_nc32_add(destination32, UINT32_C(16));
    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_SOURCE20,
        zg_nc32_add(source32, UINT32_C(20)), &word));
    ZG_NC32_TRY(zg_nc32_write(&f, ZG_NC32_STAGE_WRITE_DEST20,
        zg_nc32_add(payload16, UINT32_C(4)), word));
    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_SOURCE16,
        zg_nc32_add(source32, UINT32_C(16)), &word));
    ZG_NC32_TRY(zg_nc32_write(&f, ZG_NC32_STAGE_WRITE_DEST16, payload16, word));
    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_SOURCE24,
        zg_nc32_add(source32, UINT32_C(24)), &word));
    ZG_NC32_TRY(zg_nc32_write(&f, ZG_NC32_STAGE_WRITE_DEST24,
        zg_nc32_add(payload16, UINT32_C(8)), word));
    if (word != UINT32_C(0)) {
        ZG_NC32_TRY(zg_nc32_write(&f, ZG_NC32_STAGE_SELF_FIXUP16, payload16, payload16));
    }

    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_SOURCE12,
        zg_nc32_add(source32, UINT32_C(12)), &incoming_second));
    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_SOURCE8,
        zg_nc32_add(source32, UINT32_C(8)), &captured_data));
    if (incoming_second != UINT32_C(0)) {
        ZG_NC32_TRY(zg_nc32_xadd(&f, ZG_NC32_STAGE_INC_INCOMING_STRONG,
            zg_nc32_add(incoming_second, UINT32_C(4)), UINT32_C(1), &old_count));
        /* Native INC EDX has no later memory/branch use before flags reset. */
        sum = zg_nc32_add(old_count, UINT32_C(1));
        (void)sum;
    }
    ZG_NC32_TRY(zg_nc32_write(&f, ZG_NC32_STAGE_WRITE_DEST8,
        zg_nc32_add(destination32, UINT32_C(8)), captured_data));
    ZG_NC32_TRY(zg_nc32_read(&f, ZG_NC32_STAGE_READ_OLD_DEST12,
        zg_nc32_add(destination32, UINT32_C(12)), &old_second));
    ZG_NC32_TRY(zg_nc32_write(&f, ZG_NC32_STAGE_WRITE_DEST12,
        zg_nc32_add(destination32, UINT32_C(12)), incoming_second));
    if (old_second != UINT32_C(0)) {
        ZG_NC32_TRY(zg_nc32_xadd(&f, ZG_NC32_STAGE_DEC_OLD_STRONG,
            zg_nc32_add(old_second, UINT32_C(4)), UINT32_C(0xffffffff), &old_count));
        sum = zg_nc32_add(old_count, UINT32_C(0xffffffff));
        if (sum == UINT32_C(0)) {
            ZG_NC32_TRY(zg_nc32_call(&f, old_second, UINT32_C(4),
                ZG_NC32_STAGE_READ_SECOND_VTABLE4, ZG_NC32_STAGE_READ_SECOND_SLOT4,
                ZG_NC32_STAGE_CALL_SECOND_SLOT4, f.ops.call_slot4));
            ZG_NC32_TRY(zg_nc32_xadd(&f, ZG_NC32_STAGE_DEC_WEAK_AFTER_SLOT4,
                zg_nc32_add(old_second, UINT32_C(8)), UINT32_C(0xffffffff), &old_count));
            /* Native EBP contains the XADD OLD word; DEC sets branch flags. */
            decremented_returned_old = zg_nc32_add(old_count, UINT32_C(0xffffffff));
            if (decremented_returned_old == UINT32_C(0)) {
                ZG_NC32_TRY(zg_nc32_call(&f, old_second, UINT32_C(8),
                    ZG_NC32_STAGE_READ_SECOND_VTABLE8, ZG_NC32_STAGE_READ_SECOND_SLOT8,
                    ZG_NC32_STAGE_CALL_SECOND_SLOT8, f.ops.call_slot8));
            }
        }
    }
    result.state = ZG_NC32_RETURNED;
    result.reason = ZG_NC32_REASON_NONE;
    result.stage = ZG_NC32_STAGE_DONE;
    result.return_value32 = destination32;
    return result;

stopped:
    result.state = state;
    result.reason = f.reason;
    result.stage = f.stage;
    return result;
}

#undef ZG_NC32_TRY
