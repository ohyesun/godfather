/* Portable C11 translation of the frozen 79-instruction normal-path slice.
 *
 * Authority: ROOT-copy335-raw-normal-frame-CONTRACT-writing-selection-20261027-v1
 * and the frozen version-1 header, CONTRACT.json, INTERFACE.md and projection v4.
 * This implements the selected logical memory/stop policy. The two lower bodies
 * and exception handler 0x101bfcb5 remain opaque. It is not a native x86 ABI.
 *
 * Guest addresses are uint32_t values. Only the callbacks translate guest RAM.
 * All C pointers below refer to ordinary, valid host control/storage objects.
 * No process-wide mutable state or allocator is used. Struct padding is not
 * inspected, serialized, or included in any logical observation.
 */
#include "zg_copy335_raw_normal_frame.h"

typedef struct zg335_run {
    zg335_services services;
    zg335_result result;
} zg335_run;

static uint32_t zg335_add32(uint32_t left, uint32_t right)
{
    return (uint32_t)((uintmax_t)left + (uintmax_t)right);
}

static uint32_t zg335_sub32(uint32_t left, uint32_t right)
{
    return (uint32_t)((uintmax_t)left - (uintmax_t)right);
}

static uint32_t zg335_from_le4(const uint8_t *bytes)
{
    return (uint32_t)((uintmax_t)bytes[0]
        | ((uintmax_t)bytes[1] << 8)
        | ((uintmax_t)bytes[2] << 16)
        | ((uintmax_t)bytes[3] << 24));
}

static void zg335_to_le4(uint8_t *bytes, uint32_t word)
{
    bytes[0] = (uint8_t)((uintmax_t)word & UINT32_C(0xff));
    bytes[1] = (uint8_t)(((uintmax_t)word >> 8) & UINT32_C(0xff));
    bytes[2] = (uint8_t)(((uintmax_t)word >> 16) & UINT32_C(0xff));
    bytes[3] = (uint8_t)(((uintmax_t)word >> 24) & UINT32_C(0xff));
}

static void zg335_complete(zg335_run *run, uint32_t pc)
{
    run->result.last_completed_pc_present = UINT32_C(1);
    run->result.last_completed_pc32 = pc;
}

static int zg335_stop(zg335_run *run, uint32_t state,
    uint32_t reason, uint32_t pc)
{
    run->result.state32 = state;
    run->result.reason32 = reason;
    run->result.origin_pc_present = UINT32_C(1);
    run->result.origin_pc32 = pc;
    return 0;
}

/* This helper records only an outer instruction's request. In particular the
 * lower callbacks receive services.memory directly, without an observer shim.
 */
static int zg335_read(zg335_run *run, uint32_t pc, uint32_t space,
    uint32_t address, uint32_t width, zg335_block16 *loaded)
{
    zg335_read_reply reply = {0};
    zg335_memory_event event = {0};
    uint32_t expected_mask;
    uint32_t i;

    event.kind32 = ZG335_EVENT_READ;
    event.space32 = space;
    event.address32 = address;
    event.width32 = width;
    run->services.memory.read(run->services.memory.user,
        space, address, width, &reply);
    event.read_reply = reply;
    run->result.last_memory = event;
    run->result.last_memory_present = UINT32_C(1);

    /* Recognized non-OK states win over success-only payload validation. */
    switch (reply.state32) {
    case ZG335_SERVICE_STOPPED:
        return zg335_stop(run, ZG335_MEMORY_STOPPED, ZG335_REASON_NONE, pc);
    case ZG335_SERVICE_INVALID:
        return zg335_stop(run, ZG335_MEMORY_INVALID, ZG335_REASON_NONE, pc);
    case ZG335_SERVICE_UNKNOWN:
        return zg335_stop(run, ZG335_MEMORY_UNKNOWN, ZG335_REASON_NONE, pc);
    case ZG335_SERVICE_OK:
        break;
    default:
        return zg335_stop(run, ZG335_MEMORY_REPLY_INVALID,
            ZG335_REASON_SERVICE_STATE, pc);
    }

    /* Internal callers use only widths 4, 8 and 16. There is no shift by 32. */
    expected_mask = (uint32_t)(((uintmax_t)1 << width) - (uintmax_t)1);
    if (reply.initialized_mask32 != expected_mask) {
        return zg335_stop(run, ZG335_MEMORY_REPLY_INVALID,
            ZG335_REASON_READ_INITIALIZATION, pc);
    }
    for (i = width; i < UINT32_C(16); ++i) {
        if (reply.data.byte[i] != 0) {
            return zg335_stop(run, ZG335_MEMORY_REPLY_INVALID,
                ZG335_REASON_READ_INITIALIZATION, pc);
        }
    }
    *loaded = reply.data;
    return 1;
}

static int zg335_write(zg335_run *run, uint32_t pc, uint32_t space,
    uint32_t address, uint32_t width, zg335_block16 data)
{
    zg335_write_reply reply = {0};
    zg335_memory_event event = {0};

    event.kind32 = ZG335_EVENT_WRITE;
    event.space32 = space;
    event.address32 = address;
    event.width32 = width;
    event.write_data = data;
    run->services.memory.write(run->services.memory.user,
        space, address, width, data, &reply);
    event.write_reply = reply;
    run->result.last_memory = event;
    run->result.last_memory_present = UINT32_C(1);

    switch (reply.state32) {
    case ZG335_SERVICE_STOPPED:
        return zg335_stop(run, ZG335_MEMORY_STOPPED, ZG335_REASON_NONE, pc);
    case ZG335_SERVICE_INVALID:
        return zg335_stop(run, ZG335_MEMORY_INVALID, ZG335_REASON_NONE, pc);
    case ZG335_SERVICE_UNKNOWN:
        return zg335_stop(run, ZG335_MEMORY_UNKNOWN, ZG335_REASON_NONE, pc);
    case ZG335_SERVICE_OK:
        break;
    default:
        return zg335_stop(run, ZG335_MEMORY_REPLY_INVALID,
            ZG335_REASON_SERVICE_STATE, pc);
    }
    if (reply.committed_prefix32 != width) {
        return zg335_stop(run, ZG335_MEMORY_REPLY_INVALID,
            ZG335_REASON_WRITE_PREFIX, pc);
    }
    return 1;
}

static int zg335_load_word(zg335_run *run, uint32_t pc, uint32_t space,
    uint32_t address, uint32_t *destination)
{
    zg335_block16 loaded = {{0}};
    if (!zg335_read(run, pc, space, address, UINT32_C(4), &loaded)) {
        return 0;
    }
    *destination = zg335_from_le4(loaded.byte);
    zg335_complete(run, pc);
    return 1;
}

static int zg335_store_word(zg335_run *run, uint32_t pc, uint32_t space,
    uint32_t address, uint32_t word)
{
    zg335_block16 data = {{0}};
    zg335_to_le4(data.byte, word);
    if (!zg335_write(run, pc, space, address, UINT32_C(4), data)) {
        return 0;
    }
    zg335_complete(run, pc);
    return 1;
}

static int zg335_store_byte(zg335_run *run, uint32_t pc,
    uint32_t address, uint8_t byte)
{
    zg335_block16 data = {{0}};
    data.byte[0] = byte;
    if (!zg335_write(run, pc, ZG335_SPACE_RAM, address, UINT32_C(1), data)) {
        return 0;
    }
    zg335_complete(run, pc);
    return 1;
}

static int zg335_push(zg335_run *run, uint32_t pc, uint32_t value)
{
    /* value was evaluated before this ESP mutation. A stopped write keeps it. */
    run->result.regs.esp = zg335_sub32(run->result.regs.esp, UINT32_C(4));
    return zg335_store_word(run, pc, ZG335_SPACE_RAM,
        run->result.regs.esp, value);
}

static int zg335_pop(zg335_run *run, uint32_t pc, uint32_t *destination)
{
    zg335_block16 loaded = {{0}};
    if (!zg335_read(run, pc, ZG335_SPACE_RAM,
        run->result.regs.esp, UINT32_C(4), &loaded)) {
        return 0;
    }
    *destination = zg335_from_le4(loaded.byte);
    run->result.regs.esp = zg335_add32(run->result.regs.esp, UINT32_C(4));
    zg335_complete(run, pc);
    return 1;
}

static int zg335_vector_load(zg335_run *run, uint32_t pc,
    uint32_t address, uint32_t width)
{
    zg335_block16 loaded = {{0}};
    zg335_xmm0 next = {0};
    if (!zg335_read(run, pc, ZG335_SPACE_RAM, address, width, &loaded)) {
        return 0;
    }
    next.w0 = zg335_from_le4(&loaded.byte[0]);
    next.w1 = zg335_from_le4(&loaded.byte[4]);
    if (width == UINT32_C(16)) {
        next.w2 = zg335_from_le4(&loaded.byte[8]);
        next.w3 = zg335_from_le4(&loaded.byte[12]);
    }
    /* A successful MOVQ load leaves next.w2/w3 zero, not the previous high64. */
    run->result.xmm0 = next;
    zg335_complete(run, pc);
    return 1;
}

static int zg335_vector_store(zg335_run *run, uint32_t pc,
    uint32_t address, uint32_t width)
{
    zg335_block16 data = {{0}};
    zg335_to_le4(&data.byte[0], run->result.xmm0.w0);
    zg335_to_le4(&data.byte[4], run->result.xmm0.w1);
    if (width == UINT32_C(16)) {
        zg335_to_le4(&data.byte[8], run->result.xmm0.w2);
        zg335_to_le4(&data.byte[12], run->result.xmm0.w3);
    }
    if (!zg335_write(run, pc, ZG335_SPACE_RAM, address, width, data)) {
        return 0;
    }
    zg335_complete(run, pc);
    return 1;
}

static void zg335_endpoint_snapshots(zg335_run *run,
    const zg335_lower_reply *reply)
{
    /* A malformed raw presence remains in last_lower_reply, while unknown
     * consumer snapshots are absent with recursively zero logical payloads.
     */
    run->result.regs_present = UINT32_C(0);
    run->result.regs = (zg335_regs8){0};
    run->result.xmm0_present = UINT32_C(0);
    run->result.xmm0 = (zg335_xmm0){0};
    if (reply->regs_present == UINT32_C(1)) {
        run->result.regs_present = UINT32_C(1);
        run->result.regs = reply->returned;
    }
    if (reply->xmm0_present == UINT32_C(1)) {
        run->result.xmm0_present = UINT32_C(1);
        run->result.xmm0 = reply->xmm0_returned;
    }
}

static int zg335_call_lower(zg335_run *run, uint32_t ordinal,
    uint32_t pc, uint32_t target, uint32_t continuation,
    zg335_lower_fn lower, void *lower_user)
{
    zg335_block16 data = {{0}};
    zg335_call request = {0};
    zg335_lower_reply reply = {0};
    uint32_t expected_esp;

    /* CALL's own return-cell write does not complete the CALL instruction. */
    run->result.regs.esp = zg335_sub32(run->result.regs.esp, UINT32_C(4));
    zg335_to_le4(data.byte, continuation);
    if (!zg335_write(run, pc, ZG335_SPACE_RAM,
        run->result.regs.esp, UINT32_C(4), data)) {
        return 0;
    }
    request.ordinal32 = ordinal;
    request.call_pc32 = pc;
    request.target32 = target;
    request.continuation32 = continuation;
    request.entry = run->result.regs;
    request.xmm0_entry = run->result.xmm0;
    run->result.last_call = request;
    run->result.last_call_present = UINT32_C(1);
    ++run->result.calls_dispatched32;
    lower(lower_user, request, &run->services.memory, &reply);
    run->result.last_lower_reply = reply;
    run->result.last_lower_reply_present = UINT32_C(1);
    zg335_endpoint_snapshots(run, &reply);

    /* No return-control fields are inspected for recognized nonreturn states. */
    switch (reply.state32) {
    case ZG335_LOWER_STOPPED:
        return zg335_stop(run, ZG335_CALLEE_STOPPED, ZG335_REASON_NONE, pc);
    case ZG335_LOWER_INVALID:
        return zg335_stop(run, ZG335_CALLEE_INVALID, ZG335_REASON_NONE, pc);
    case ZG335_LOWER_UNKNOWN:
        return zg335_stop(run, ZG335_CALLEE_UNKNOWN, ZG335_REASON_NONE, pc);
    case ZG335_LOWER_RETURNED:
        break;
    default:
        return zg335_stop(run, ZG335_CALLEE_REPLY_INVALID,
            ZG335_REASON_LOWER_STATE, pc);
    }
    if (reply.regs_present != UINT32_C(1)
        || reply.xmm0_present != UINT32_C(1)
        || reply.returned_to_present != UINT32_C(1)
        || reply.cleanup_present != UINT32_C(1)) {
        return zg335_stop(run, ZG335_CALLEE_REPLY_INVALID,
            ZG335_REASON_RETURNED_PRESENCE, pc);
    }
    if (reply.returned_to32 != continuation) {
        return zg335_stop(run, ZG335_RETURN_CONTROL_INVALID,
            ZG335_REASON_RETURN_TARGET, pc);
    }
    if (reply.ret_cleanup32 > UINT32_C(65535)) {
        return zg335_stop(run, ZG335_RETURN_CONTROL_INVALID,
            ZG335_REASON_CLEANUP_RANGE, pc);
    }
    expected_esp = zg335_add32(zg335_add32(request.entry.esp, UINT32_C(4)),
        reply.ret_cleanup32);
    if (reply.returned.esp != expected_esp) {
        return zg335_stop(run, ZG335_RETURN_CONTROL_INVALID,
            ZG335_REASON_RETURN_ESP, pc);
    }
    ++run->result.calls_continued32;
    zg335_complete(run, pc);
    return 1;
}

zg335_result zg335_raw_normal_frame(zg335_entry entry,
    const zg335_services *services)
{
    zg335_run run = {0};
    zg335_block16 final_cell = {{0}};

    if (services == 0) {
        run.result.state32 = ZG335_BAD_INTERFACE;
        run.result.reason32 = ZG335_REASON_REQUIRED_HOOK;
        return run.result;
    }
    /* This is the one read of the original host table. Nested calls own their
     * own run. Later callbacks may mutate the original table without changing
     * this invocation's captured function pointers or userdata.
     */
    run.services = *services;
    if (run.services.version32 != ZG335_VERSION) {
        run.result.state32 = ZG335_BAD_INTERFACE;
        run.result.reason32 = ZG335_REASON_INTERFACE_VERSION;
        return run.result;
    }
    if (run.services.memory.read == 0 || run.services.memory.write == 0
        || run.services.e4c0 == 0 || run.services.fe0 == 0) {
        run.result.state32 = ZG335_BAD_INTERFACE;
        run.result.reason32 = ZG335_REASON_REQUIRED_HOOK;
        return run.result;
    }
    if (entry.regs_present != UINT32_C(1)
        || entry.xmm0_present != UINT32_C(1)) {
        run.result.state32 = ZG335_BAD_ENTRY;
        run.result.reason32 = ZG335_REASON_ENTRY_PRESENCE;
        return run.result;
    }
    run.result.entry_esp_present = UINT32_C(1);
    run.result.entry_esp32 = entry.regs.esp;
    run.result.regs_present = UINT32_C(1);
    run.result.regs = entry.regs;
    run.result.xmm0_present = UINT32_C(1);
    run.result.xmm0 = entry.xmm0;

    /* @0x1001fc90 PUSH -0x1 */
    if (!zg335_push(&run, UINT32_C(0x1001fc90), UINT32_C(0xffffffff))) {
        return run.result;
    }
    /* @0x1001fc92 PUSH 0x101bfcb5 */
    if (!zg335_push(&run, UINT32_C(0x1001fc92), UINT32_C(0x101bfcb5))) {
        return run.result;
    }
    /* @0x1001fc97 MOV EAX, FS:[0x0] */
    if (!zg335_load_word(&run, UINT32_C(0x1001fc97), ZG335_SPACE_LOGICAL_FS,
        UINT32_C(0), &run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fc9d PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fc9d), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fc9e MOV dword ptr FS:[0x0], ESP */
    if (!zg335_store_word(&run, UINT32_C(0x1001fc9e), ZG335_SPACE_LOGICAL_FS,
        UINT32_C(0), run.result.regs.esp)) {
        return run.result;
    }
    /* @0x1001fca5 PUSH ECX */
    if (!zg335_push(&run, UINT32_C(0x1001fca5), run.result.regs.ecx)) {
        return run.result;
    }
    /* @0x1001fca6 PUSH ESI */
    if (!zg335_push(&run, UINT32_C(0x1001fca6), run.result.regs.esi)) {
        return run.result;
    }
    /* @0x1001fca7 PUSH EDI */
    if (!zg335_push(&run, UINT32_C(0x1001fca7), run.result.regs.edi)) {
        return run.result;
    }
    /* @0x1001fca8 MOV EDI, ECX */
    run.result.regs.edi = run.result.regs.ecx;
    zg335_complete(&run, UINT32_C(0x1001fca8));
    /* @0x1001fcaa MOV dword ptr [ESP + 0x8], EDI */
    if (!zg335_store_word(&run, UINT32_C(0x1001fcaa), ZG335_SPACE_RAM,
        zg335_add32(run.result.regs.esp, UINT32_C(0x8)), run.result.regs.edi)) {
        return run.result;
    }
    /* @0x1001fcae MOV ESI, dword ptr [ESP + 0x1c] */
    if (!zg335_load_word(&run, UINT32_C(0x1001fcae), ZG335_SPACE_RAM,
        zg335_add32(run.result.regs.esp, UINT32_C(0x1c)), &run.result.regs.esi)) {
        return run.result;
    }
    /* @0x1001fcb2 LEA ECX, [EDI + 0x4] */
    run.result.regs.ecx = zg335_add32(run.result.regs.edi, UINT32_C(0x4));
    zg335_complete(&run, UINT32_C(0x1001fcb2));
    /* @0x1001fcb5 MOV dword ptr [ESP + 0x14], 0x0 */
    if (!zg335_store_word(&run, UINT32_C(0x1001fcb5), ZG335_SPACE_RAM,
        zg335_add32(run.result.regs.esp, UINT32_C(0x14)), UINT32_C(0))) {
        return run.result;
    }
    /* @0x1001fcbd MOV dword ptr [EDI], 0x101d7bcc */
    if (!zg335_store_word(&run, UINT32_C(0x1001fcbd), ZG335_SPACE_RAM,
        run.result.regs.edi, UINT32_C(0x101d7bcc))) {
        return run.result;
    }
    /* @0x1001fcc3 LEA EAX, [ESI + 0x4] */
    run.result.regs.eax = zg335_add32(run.result.regs.esi, UINT32_C(0x4));
    zg335_complete(&run, UINT32_C(0x1001fcc3));
    /* @0x1001fcc6 PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fcc6), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fcc7 CALL 0x1001e4c0 */
    if (!zg335_call_lower(&run, UINT32_C(1), UINT32_C(0x1001fcc7),
        UINT32_C(0x1001e4c0), UINT32_C(0x1001fccc),
        run.services.e4c0, run.services.e4c0_user)) {
        return run.result;
    }
    /* @0x1001fccc LEA EAX, [ESI + 0xc] */
    run.result.regs.eax = zg335_add32(run.result.regs.esi, UINT32_C(0xc));
    zg335_complete(&run, UINT32_C(0x1001fccc));
    /* @0x1001fccf MOV byte ptr [ESP + 0x14], 0x1 */
    if (!zg335_store_byte(&run, UINT32_C(0x1001fccf),
        zg335_add32(run.result.regs.esp, UINT32_C(0x14)), UINT8_C(1))) {
        return run.result;
    }
    /* @0x1001fcd4 LEA ECX, [EDI + 0xc] */
    run.result.regs.ecx = zg335_add32(run.result.regs.edi, UINT32_C(0xc));
    zg335_complete(&run, UINT32_C(0x1001fcd4));
    /* @0x1001fcd7 PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fcd7), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fcd8 CALL 0x1001e4c0 */
    if (!zg335_call_lower(&run, UINT32_C(2), UINT32_C(0x1001fcd8),
        UINT32_C(0x1001e4c0), UINT32_C(0x1001fcdd),
        run.services.e4c0, run.services.e4c0_user)) {
        return run.result;
    }
    /* @0x1001fcdd LEA EAX, [ESI + 0x14] */
    run.result.regs.eax = zg335_add32(run.result.regs.esi, UINT32_C(0x14));
    zg335_complete(&run, UINT32_C(0x1001fcdd));
    /* @0x1001fce0 MOV byte ptr [ESP + 0x14], 0x2 */
    if (!zg335_store_byte(&run, UINT32_C(0x1001fce0),
        zg335_add32(run.result.regs.esp, UINT32_C(0x14)), UINT8_C(2))) {
        return run.result;
    }
    /* @0x1001fce5 LEA ECX, [EDI + 0x14] */
    run.result.regs.ecx = zg335_add32(run.result.regs.edi, UINT32_C(0x14));
    zg335_complete(&run, UINT32_C(0x1001fce5));
    /* @0x1001fce8 PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fce8), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fce9 CALL 0x10007fe0 */
    if (!zg335_call_lower(&run, UINT32_C(3), UINT32_C(0x1001fce9),
        UINT32_C(0x10007fe0), UINT32_C(0x1001fcee),
        run.services.fe0, run.services.fe0_user)) {
        return run.result;
    }
    /* @0x1001fcee LEA EAX, [ESI + 0x1c] */
    run.result.regs.eax = zg335_add32(run.result.regs.esi, UINT32_C(0x1c));
    zg335_complete(&run, UINT32_C(0x1001fcee));
    /* @0x1001fcf1 MOV byte ptr [ESP + 0x14], 0x3 */
    if (!zg335_store_byte(&run, UINT32_C(0x1001fcf1),
        zg335_add32(run.result.regs.esp, UINT32_C(0x14)), UINT8_C(3))) {
        return run.result;
    }
    /* @0x1001fcf6 LEA ECX, [EDI + 0x1c] */
    run.result.regs.ecx = zg335_add32(run.result.regs.edi, UINT32_C(0x1c));
    zg335_complete(&run, UINT32_C(0x1001fcf6));
    /* @0x1001fcf9 PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fcf9), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fcfa CALL 0x10007fe0 */
    if (!zg335_call_lower(&run, UINT32_C(4), UINT32_C(0x1001fcfa),
        UINT32_C(0x10007fe0), UINT32_C(0x1001fcff),
        run.services.fe0, run.services.fe0_user)) {
        return run.result;
    }
    /* @0x1001fcff LEA EAX, [ESI + 0x24] */
    run.result.regs.eax = zg335_add32(run.result.regs.esi, UINT32_C(0x24));
    zg335_complete(&run, UINT32_C(0x1001fcff));
    /* @0x1001fd02 MOV byte ptr [ESP + 0x14], 0x4 */
    if (!zg335_store_byte(&run, UINT32_C(0x1001fd02),
        zg335_add32(run.result.regs.esp, UINT32_C(0x14)), UINT8_C(4))) {
        return run.result;
    }
    /* @0x1001fd07 LEA ECX, [EDI + 0x24] */
    run.result.regs.ecx = zg335_add32(run.result.regs.edi, UINT32_C(0x24));
    zg335_complete(&run, UINT32_C(0x1001fd07));
    /* @0x1001fd0a PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fd0a), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fd0b CALL 0x10007fe0 */
    if (!zg335_call_lower(&run, UINT32_C(5), UINT32_C(0x1001fd0b),
        UINT32_C(0x10007fe0), UINT32_C(0x1001fd10),
        run.services.fe0, run.services.fe0_user)) {
        return run.result;
    }
    /* @0x1001fd10 LEA EAX, [ESI + 0x2c] */
    run.result.regs.eax = zg335_add32(run.result.regs.esi, UINT32_C(0x2c));
    zg335_complete(&run, UINT32_C(0x1001fd10));
    /* @0x1001fd13 MOV byte ptr [ESP + 0x14], 0x5 */
    if (!zg335_store_byte(&run, UINT32_C(0x1001fd13),
        zg335_add32(run.result.regs.esp, UINT32_C(0x14)), UINT8_C(5))) {
        return run.result;
    }
    /* @0x1001fd18 LEA ECX, [EDI + 0x2c] */
    run.result.regs.ecx = zg335_add32(run.result.regs.edi, UINT32_C(0x2c));
    zg335_complete(&run, UINT32_C(0x1001fd18));
    /* @0x1001fd1b PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fd1b), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fd1c CALL 0x10007fe0 */
    if (!zg335_call_lower(&run, UINT32_C(6), UINT32_C(0x1001fd1c),
        UINT32_C(0x10007fe0), UINT32_C(0x1001fd21),
        run.services.fe0, run.services.fe0_user)) {
        return run.result;
    }
    /* @0x1001fd21 LEA EAX, [ESI + 0x34] */
    run.result.regs.eax = zg335_add32(run.result.regs.esi, UINT32_C(0x34));
    zg335_complete(&run, UINT32_C(0x1001fd21));
    /* @0x1001fd24 MOV byte ptr [ESP + 0x14], 0x6 */
    if (!zg335_store_byte(&run, UINT32_C(0x1001fd24),
        zg335_add32(run.result.regs.esp, UINT32_C(0x14)), UINT8_C(6))) {
        return run.result;
    }
    /* @0x1001fd29 LEA ECX, [EDI + 0x34] */
    run.result.regs.ecx = zg335_add32(run.result.regs.edi, UINT32_C(0x34));
    zg335_complete(&run, UINT32_C(0x1001fd29));
    /* @0x1001fd2c PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fd2c), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fd2d CALL 0x10007fe0 */
    if (!zg335_call_lower(&run, UINT32_C(7), UINT32_C(0x1001fd2d),
        UINT32_C(0x10007fe0), UINT32_C(0x1001fd32),
        run.services.fe0, run.services.fe0_user)) {
        return run.result;
    }
    /* @0x1001fd32 LEA EAX, [ESI + 0x3c] */
    run.result.regs.eax = zg335_add32(run.result.regs.esi, UINT32_C(0x3c));
    zg335_complete(&run, UINT32_C(0x1001fd32));
    /* @0x1001fd35 MOV byte ptr [ESP + 0x14], 0x7 */
    if (!zg335_store_byte(&run, UINT32_C(0x1001fd35),
        zg335_add32(run.result.regs.esp, UINT32_C(0x14)), UINT8_C(7))) {
        return run.result;
    }
    /* @0x1001fd3a PUSH EAX */
    if (!zg335_push(&run, UINT32_C(0x1001fd3a), run.result.regs.eax)) {
        return run.result;
    }
    /* @0x1001fd3b LEA ECX, [EDI + 0x3c] */
    run.result.regs.ecx = zg335_add32(run.result.regs.edi, UINT32_C(0x3c));
    zg335_complete(&run, UINT32_C(0x1001fd3b));
    /* @0x1001fd3e CALL 0x10007fe0 */
    if (!zg335_call_lower(&run, UINT32_C(8), UINT32_C(0x1001fd3e),
        UINT32_C(0x10007fe0), UINT32_C(0x1001fd43),
        run.services.fe0, run.services.fe0_user)) {
        return run.result;
    }
    /* @0x1001fd43 MOV dword ptr [EDI + 0x44], 0x101d7b58 */
    if (!zg335_store_word(&run, UINT32_C(0x1001fd43), ZG335_SPACE_RAM,
        zg335_add32(run.result.regs.edi, UINT32_C(0x44)), UINT32_C(0x101d7b58))) {
        return run.result;
    }
    /* @0x1001fd4a MOV EAX, EDI */
    run.result.regs.eax = run.result.regs.edi;
    zg335_complete(&run, UINT32_C(0x1001fd4a));
    /* @0x1001fd4c MOVDQU XMM0, xmmword ptr [ESI + 0x48] */
    if (!zg335_vector_load(&run, UINT32_C(0x1001fd4c),
        zg335_add32(run.result.regs.esi, UINT32_C(0x48)), UINT32_C(16))) {
        return run.result;
    }
    /* @0x1001fd51 MOV ECX, dword ptr [ESP + 0xc] */
    if (!zg335_load_word(&run, UINT32_C(0x1001fd51), ZG335_SPACE_RAM,
        zg335_add32(run.result.regs.esp, UINT32_C(0xc)), &run.result.regs.ecx)) {
        return run.result;
    }
    /* @0x1001fd55 MOVDQU xmmword ptr [EDI + 0x48], XMM0 */
    if (!zg335_vector_store(&run, UINT32_C(0x1001fd55),
        zg335_add32(run.result.regs.edi, UINT32_C(0x48)), UINT32_C(16))) {
        return run.result;
    }
    /* @0x1001fd5a MOVQ XMM0, qword ptr [ESI + 0x58] */
    if (!zg335_vector_load(&run, UINT32_C(0x1001fd5a),
        zg335_add32(run.result.regs.esi, UINT32_C(0x58)), UINT32_C(8))) {
        return run.result;
    }
    /* @0x1001fd5f MOVQ qword ptr [EDI + 0x58], XMM0 */
    if (!zg335_vector_store(&run, UINT32_C(0x1001fd5f),
        zg335_add32(run.result.regs.edi, UINT32_C(0x58)), UINT32_C(8))) {
        return run.result;
    }
    /* @0x1001fd64 MOV dword ptr [EDI + 0x60], 0x101d7b58 */
    if (!zg335_store_word(&run, UINT32_C(0x1001fd64), ZG335_SPACE_RAM,
        zg335_add32(run.result.regs.edi, UINT32_C(0x60)), UINT32_C(0x101d7b58))) {
        return run.result;
    }
    /* @0x1001fd6b MOVDQU XMM0, xmmword ptr [ESI + 0x64] */
    if (!zg335_vector_load(&run, UINT32_C(0x1001fd6b),
        zg335_add32(run.result.regs.esi, UINT32_C(0x64)), UINT32_C(16))) {
        return run.result;
    }
    /* @0x1001fd70 MOVDQU xmmword ptr [EDI + 0x64], XMM0 */
    if (!zg335_vector_store(&run, UINT32_C(0x1001fd70),
        zg335_add32(run.result.regs.edi, UINT32_C(0x64)), UINT32_C(16))) {
        return run.result;
    }
    /* @0x1001fd75 MOVQ XMM0, qword ptr [ESI + 0x74] */
    if (!zg335_vector_load(&run, UINT32_C(0x1001fd75),
        zg335_add32(run.result.regs.esi, UINT32_C(0x74)), UINT32_C(8))) {
        return run.result;
    }
    /* @0x1001fd7a MOVQ qword ptr [EDI + 0x74], XMM0 */
    if (!zg335_vector_store(&run, UINT32_C(0x1001fd7a),
        zg335_add32(run.result.regs.edi, UINT32_C(0x74)), UINT32_C(8))) {
        return run.result;
    }
    /* @0x1001fd7f MOV dword ptr [EDI + 0x7c], 0x101d7b58 */
    if (!zg335_store_word(&run, UINT32_C(0x1001fd7f), ZG335_SPACE_RAM,
        zg335_add32(run.result.regs.edi, UINT32_C(0x7c)), UINT32_C(0x101d7b58))) {
        return run.result;
    }
    /* @0x1001fd86 MOVDQU XMM0, xmmword ptr [ESI + 0x80] */
    if (!zg335_vector_load(&run, UINT32_C(0x1001fd86),
        zg335_add32(run.result.regs.esi, UINT32_C(0x80)), UINT32_C(16))) {
        return run.result;
    }
    /* @0x1001fd8e MOVDQU xmmword ptr [EDI + 0x80], XMM0 */
    if (!zg335_vector_store(&run, UINT32_C(0x1001fd8e),
        zg335_add32(run.result.regs.edi, UINT32_C(0x80)), UINT32_C(16))) {
        return run.result;
    }
    /* @0x1001fd96 MOVQ XMM0, qword ptr [ESI + 0x90] */
    if (!zg335_vector_load(&run, UINT32_C(0x1001fd96),
        zg335_add32(run.result.regs.esi, UINT32_C(0x90)), UINT32_C(8))) {
        return run.result;
    }
    /* @0x1001fd9e MOVQ qword ptr [EDI + 0x90], XMM0 */
    if (!zg335_vector_store(&run, UINT32_C(0x1001fd9e),
        zg335_add32(run.result.regs.edi, UINT32_C(0x90)), UINT32_C(8))) {
        return run.result;
    }
    /* @0x1001fda6 MOV dword ptr [EDI + 0x98], 0x101d7b58 */
    if (!zg335_store_word(&run, UINT32_C(0x1001fda6), ZG335_SPACE_RAM,
        zg335_add32(run.result.regs.edi, UINT32_C(0x98)), UINT32_C(0x101d7b58))) {
        return run.result;
    }
    /* @0x1001fdb0 MOVDQU XMM0, xmmword ptr [ESI + 0x9c] */
    if (!zg335_vector_load(&run, UINT32_C(0x1001fdb0),
        zg335_add32(run.result.regs.esi, UINT32_C(0x9c)), UINT32_C(16))) {
        return run.result;
    }
    /* @0x1001fdb8 MOVDQU xmmword ptr [EDI + 0x9c], XMM0 */
    if (!zg335_vector_store(&run, UINT32_C(0x1001fdb8),
        zg335_add32(run.result.regs.edi, UINT32_C(0x9c)), UINT32_C(16))) {
        return run.result;
    }
    /* @0x1001fdc0 MOVQ XMM0, qword ptr [ESI + 0xac] */
    if (!zg335_vector_load(&run, UINT32_C(0x1001fdc0),
        zg335_add32(run.result.regs.esi, UINT32_C(0xac)), UINT32_C(8))) {
        return run.result;
    }
    /* @0x1001fdc8 MOVQ qword ptr [EDI + 0xac], XMM0 */
    if (!zg335_vector_store(&run, UINT32_C(0x1001fdc8),
        zg335_add32(run.result.regs.edi, UINT32_C(0xac)), UINT32_C(8))) {
        return run.result;
    }
    /* @0x1001fdd0 POP EDI */
    if (!zg335_pop(&run, UINT32_C(0x1001fdd0), &run.result.regs.edi)) {
        return run.result;
    }
    /* @0x1001fdd1 POP ESI */
    if (!zg335_pop(&run, UINT32_C(0x1001fdd1), &run.result.regs.esi)) {
        return run.result;
    }
    /* @0x1001fdd2 MOV dword ptr FS:[0x0], ECX */
    if (!zg335_store_word(&run, UINT32_C(0x1001fdd2), ZG335_SPACE_LOGICAL_FS,
        UINT32_C(0), run.result.regs.ecx)) {
        return run.result;
    }
    /* @0x1001fdd9 ADD ESP, 0x10 */
    run.result.regs.esp = zg335_add32(run.result.regs.esp, UINT32_C(0x10));
    zg335_complete(&run, UINT32_C(0x1001fdd9));
    /* @0x1001fddc RET 0x4 */
    if (!zg335_read(&run, UINT32_C(0x1001fddc), ZG335_SPACE_RAM,
        run.result.regs.esp, UINT32_C(4), &final_cell)) {
        return run.result;
    }
    run.result.final_target32 = zg335_from_le4(final_cell.byte);
    run.result.final_target_present = UINT32_C(1);
    run.result.regs.esp = zg335_add32(run.result.regs.esp, UINT32_C(8));
    zg335_complete(&run, UINT32_C(0x1001fddc));
    run.result.state32 = ZG335_RETURNED;
    run.result.reason32 = ZG335_REASON_NONE;
    return run.result;
}
