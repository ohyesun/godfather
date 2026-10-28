#include "zg_range76e0.h"

typedef struct zg_r76_frame {
    zg_r76_services ops;
    zg_r76_reason reason;
    zg_r76_stage stage;
} zg_r76_frame;

static uint32_t zg_r76_add32(uint32_t value, uint32_t increment)
{
    /* ULL is at least 64 bits; conversion supplies modulo 2^32 even on
     * implementations whose integer promotions would otherwise be signed.
     */
    return (uint32_t)((unsigned long long)value +
                      (unsigned long long)increment);
}

static zg_r76_state zg_r76_checked(zg_r76_frame *f, zg_r76_state state)
{
    if (state == ZG_R76_RETURNED) {
        return state;
    }
    if (state == ZG_R76_NONRETURN || state == ZG_R76_ERROR) {
        f->reason = ZG_R76_REASON_SERVICE_STOP;
        return state;
    }
    f->reason = ZG_R76_REASON_INVALID_STATE;
    return ZG_R76_ERROR;
}

static zg_r76_state zg_r76_clone_checked(
    zg_r76_frame *f, zg_r76_clone_result reply, uint32_t *node_out)
{
    zg_r76_state state = zg_r76_checked(f, reply.state);
    if (state != ZG_R76_RETURNED) {
        return state;
    }
    if (reply.normal_abi_preserved != UINT32_C(1)) {
        f->reason = ZG_R76_REASON_ABI_UNCONFIRMED;
        return ZG_R76_ERROR;
    }
    *node_out = reply.eax32;
    return ZG_R76_RETURNED;
}

#define ZG_R76_TRY(expression) \
    do { \
        state = (expression); \
        if (state != ZG_R76_RETURNED) { \
            goto stopped; \
        } \
    } while (0)

zg_r76_result zg_range76e0_normal(
    const zg_r76_services *services,
    uint32_t q32,
    uint32_t argument_base32)
{
    zg_r76_frame f;
    zg_r76_result result;
    zg_r76_clone_result reply;
    zg_r76_state state;
    uint32_t current;
    uint32_t last;
    uint32_t position;
    uint32_t payload_address;
    uint32_t previous;
    uint32_t node;
    uint32_t count;
    uint32_t predecessor;
    uint32_t tail_current;
    uint32_t next;

    result.state = ZG_R76_ERROR;
    result.reason = ZG_R76_REASON_BAD_INTERFACE;
    result.stage = ZG_R76_STAGE_INTERFACE;
    result.return_value32 = UINT32_C(0);
    if (services == 0) {
        return result;
    }
    /* New host policy: snapshot and validate every hook before any effect. */
    f.ops = *services;
    if (f.ops.read32 == 0 || f.ops.write32 == 0 ||
        f.ops.clone == 0 || f.ops.length_error == 0) {
        return result;
    }
    f.reason = ZG_R76_REASON_NONE;
    f.stage = ZG_R76_STAGE_INTERFACE;

    /* 0x100076fb: one initial argument2 load, before the first comparison. */
    f.stage = ZG_R76_STAGE_READ_INITIAL_CURRENT;
    ZG_R76_TRY(zg_r76_checked(&f, f.ops.read32(
        f.ops.context, zg_r76_add32(argument_base32, UINT32_C(4)), &current)));

    for (;;) {
        /* 0x10007713: fresh argument3 on EVERY top comparison. */
        f.stage = ZG_R76_STAGE_READ_LAST;
        ZG_R76_TRY(zg_r76_checked(&f, f.ops.read32(
            f.ops.context, zg_r76_add32(argument_base32, UINT32_C(8)), &last)));
        if (current == last) {
            break;
        }

        /* 0x10007718/1b/21: nonempty argument1, payload role, prior prev. */
        f.stage = ZG_R76_STAGE_READ_POSITION;
        ZG_R76_TRY(zg_r76_checked(&f,
            f.ops.read32(f.ops.context, argument_base32, &position)));
        payload_address = zg_r76_add32(current, UINT32_C(8));
        f.stage = ZG_R76_STAGE_READ_POSITION_PREV;
        ZG_R76_TRY(zg_r76_checked(&f, f.ops.read32(
            f.ops.context, zg_r76_add32(position, UINT32_C(4)), &previous)));

        /* 0x10007725/2a: actual opaque clone return becomes node (EDX). */
        f.stage = ZG_R76_STAGE_CLONE;
        reply = f.ops.clone(f.ops.context, q32, position,
                            previous, payload_address);
        ZG_R76_TRY(zg_r76_clone_checked(&f, reply, &node));

        /* 0x10007731..39: count is fresh AFTER clone, equality only. */
        f.stage = ZG_R76_STAGE_READ_COUNT;
        ZG_R76_TRY(zg_r76_checked(&f, f.ops.read32(
            f.ops.context, zg_r76_add32(q32, UINT32_C(4)), &count)));
        if (count == ZG_R76_COUNT_LIMIT32) {
            f.stage = ZG_R76_STAGE_LENGTH_ERROR;
            state = zg_r76_checked(&f, f.ops.length_error(
                f.ops.context, ZG_R76_LENGTH_MESSAGE32));
            if (state == ZG_R76_RETURNED) {
                /* Native fallthrough uses unproved volatile EAX/EDX.
                 * This supported model ends at that explicit frontier.
                 */
                state = ZG_R76_ERROR;
                f.reason = ZG_R76_REASON_LENGTH_ERROR_RETURN_FRONTIER;
            }
            goto stopped;
        }

        /* 0x10007746/47: increment the observed count, without a re-read. */
        f.stage = ZG_R76_STAGE_WRITE_COUNT;
        ZG_R76_TRY(zg_r76_checked(&f, f.ops.write32(
            f.ops.context, zg_r76_add32(q32, UINT32_C(4)),
            zg_r76_add32(count, UINT32_C(1)))));

        /* 0x1000774a precedes 4d: node.prev may alias this very store. */
        f.stage = ZG_R76_STAGE_WRITE_POSITION_PREV;
        ZG_R76_TRY(zg_r76_checked(&f, f.ops.write32(
            f.ops.context, zg_r76_add32(position, UINT32_C(4)), node)));
        f.stage = ZG_R76_STAGE_READ_NODE_PREV;
        ZG_R76_TRY(zg_r76_checked(&f, f.ops.read32(
            f.ops.context, zg_r76_add32(node, UINT32_C(4)), &predecessor)));
        f.stage = ZG_R76_STAGE_WRITE_PREDECESSOR_NEXT;
        ZG_R76_TRY(zg_r76_checked(&f,
            f.ops.write32(f.ops.context, predecessor, node)));

        /* 0x10007752/55/57: reload argument2 AFTER the link stores, then
         * fresh current.next, then update its argument cell. Keep next in
         * the EAX role for the next top comparison; do not preload arg3.
         */
        f.stage = ZG_R76_STAGE_RELOAD_CURRENT;
        ZG_R76_TRY(zg_r76_checked(&f, f.ops.read32(
            f.ops.context, zg_r76_add32(argument_base32, UINT32_C(4)),
            &tail_current)));
        f.stage = ZG_R76_STAGE_READ_CURRENT_NEXT;
        ZG_R76_TRY(zg_r76_checked(&f,
            f.ops.read32(f.ops.context, tail_current, &next)));
        f.stage = ZG_R76_STAGE_WRITE_CURRENT;
        ZG_R76_TRY(zg_r76_checked(&f, f.ops.write32(
            f.ops.context, zg_r76_add32(argument_base32, UINT32_C(4)), next)));
        current = next;
    }

    result.state = ZG_R76_RETURNED;
    result.reason = ZG_R76_REASON_NONE;
    result.stage = ZG_R76_STAGE_DONE;
    result.return_value32 = current;
    return result;

stopped:
    result.state = state;
    result.reason = f.reason;
    result.stage = f.stage;
    return result;
}

#undef ZG_R76_TRY
