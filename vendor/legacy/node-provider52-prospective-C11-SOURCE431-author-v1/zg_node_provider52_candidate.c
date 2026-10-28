#include "zg_node_provider52_candidate.h"

typedef struct zg_np52_frame {
    zg_np52_services ops;
    zg_np52_reason reason;
    zg_np52_stage stage;
} zg_np52_frame;

static uint32_t zg_np52_add4(uint32_t value)
{
    /* ULL is at least64bits: no signed integer-promotion overflow. */
    return (uint32_t)((unsigned long long)value + 4ULL);
}

static zg_np52_state zg_np52_checked(zg_np52_frame *f, zg_np52_state state)
{
    if (state == ZG_NP52_RETURNED) {
        return state;
    }
    if (state == ZG_NP52_NONRETURN || state == ZG_NP52_ERROR) {
        f->reason = ZG_NP52_REASON_SERVICE_STOP;
        return state;
    }
    f->reason = ZG_NP52_REASON_INVALID_STATE;
    return ZG_NP52_ERROR;
}

static zg_np52_state zg_np52_call_checked(
    zg_np52_frame *f, zg_np52_call_result reply, uint32_t *eax_out)
{
    zg_np52_state state = zg_np52_checked(f, reply.state);
    if (state != ZG_NP52_RETURNED) {
        return state;
    }
    if (reply.normal_abi_preserved != UINT32_C(1)) {
        f->reason = ZG_NP52_REASON_ABI_UNCONFIRMED;
        return ZG_NP52_ERROR;
    }
    *eax_out = reply.eax32;
    return ZG_NP52_RETURNED;
}

#define ZG_NP52_TRY(expression) \
    do { \
        state = (expression); \
        if (state != ZG_NP52_RETURNED) { \
            goto stopped; \
        } \
    } while (0)

zg_np52_result zg_np52_provider_candidate(const zg_np52_services *services)
{
    zg_np52_frame f;
    zg_np52_result result;
    zg_np52_call_result reply;
    zg_np52_state state;
    uint32_t node;
    uint32_t first;
    uint32_t second;
    uint32_t second_address;

    result.state = ZG_NP52_ERROR;
    result.reason = ZG_NP52_REASON_BAD_INTERFACE;
    result.stage = ZG_NP52_STAGE_INTERFACE;
    result.return_value32 = UINT32_C(0);
    if (services == 0) {
        return result;
    }
    f.ops = *services;
    if (f.ops.allocate == 0 || f.ops.failure_import == 0 ||
        f.ops.read_argument32 == 0 || f.ops.write32 == 0) {
        return result;
    }
    f.reason = ZG_NP52_REASON_NONE;
    f.stage = ZG_NP52_STAGE_INTERFACE;

    f.stage = ZG_NP52_STAGE_ALLOCATE36;
    reply = f.ops.allocate(f.ops.context, UINT32_C(36));
    ZG_NP52_TRY(zg_np52_call_checked(&f, reply, &node));
    if (node == UINT32_C(0)) {
        f.stage = ZG_NP52_STAGE_FAILURE_IMPORT;
        reply = f.ops.failure_import(f.ops.context);
        /* A RETURNED import supplies its actual EAX and FALLS THROUGH. */
        ZG_NP52_TRY(zg_np52_call_checked(&f, reply, &node));
    }

    f.stage = ZG_NP52_STAGE_READ_ARG_A;
    ZG_NP52_TRY(zg_np52_checked(&f,
        f.ops.read_argument32(f.ops.context, UINT32_C(1), &first)));
    if (first == UINT32_C(0)) {
        first = node;
        second = node;
    } else {
        f.stage = ZG_NP52_STAGE_READ_ARG_B;
        ZG_NP52_TRY(zg_np52_checked(&f,
            f.ops.read_argument32(f.ops.context, UINT32_C(2), &second)));
    }
    f.stage = ZG_NP52_STAGE_WRITE_FIRST;
    ZG_NP52_TRY(zg_np52_checked(&f, f.ops.write32(f.ops.context, node, first)));
    second_address = zg_np52_add4(node);
    if (second_address != UINT32_C(0)) {
        f.stage = ZG_NP52_STAGE_WRITE_SECOND;
        ZG_NP52_TRY(zg_np52_checked(&f,
            f.ops.write32(f.ops.context, second_address, second)));
    }
    result.state = ZG_NP52_RETURNED;
    result.reason = ZG_NP52_REASON_NONE;
    result.stage = ZG_NP52_STAGE_DONE;
    result.return_value32 = node;
    return result;

stopped:
    result.state = state;
    result.reason = f.reason;
    result.stage = f.stage;
    return result;
}
#undef ZG_NP52_TRY
