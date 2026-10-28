#include "zg335_portable.h"

#include <stdio.h>
#include <string.h>

#define RAM_SIZE 0x100000U

typedef struct Probe {
    unsigned char ram[RAM_SIZE];
    unsigned char initialized[RAM_SIZE];
    uint32_t fs_head;
    int fs_initialized;
    uint32_t allocated[32];
    uint32_t allocated_count;
    uint32_t allocate_index;
    uint32_t machine_count;
    zg116_xmm0 current_xmm;
} Probe;

static void put8(Probe *p, uint32_t address, uint8_t value)
{
    if (address < RAM_SIZE) {
        p->ram[address] = value;
        p->initialized[address] = 1;
    }
}

static void put32(Probe *p, uint32_t address, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) {
        put8(p, address + i, (uint8_t)(value >> (8U * i)));
    }
}

static uint32_t peek32(const Probe *p, uint32_t address)
{
    uint32_t value = 0;
    uint32_t i;
    for (i = 0; i < 4; ++i) {
        if (address + i < RAM_SIZE) {
            value |= (uint32_t)p->ram[address + i] << (8U * i);
        }
    }
    return value;
}

static int read_byte(const Probe *p, uint32_t space, uint32_t address, uint8_t *out)
{
    if (space == ZG335_SPACE_LOGICAL_FS) {
        if (!p->fs_initialized || address >= 4) { return 0; }
        *out = (uint8_t)(p->fs_head >> (address * 8U));
        return 1;
    }
    if (address >= RAM_SIZE || !p->initialized[address]) { return 0; }
    *out = p->ram[address];
    return 1;
}

static void probe_read(void *user, uint32_t space, uint32_t address,
    uint32_t width, zg335_read_reply *reply)
{
    Probe *p = (Probe *)user;
    uint32_t i;
    int complete = width == 1 || width == 2 || width == 4 || width == 8 || width == 16;
    *reply = (zg335_read_reply){0};
    for (i = 0; i < width; ++i) {
        uint8_t value = 0;
        if (!read_byte(p, space, address + i, &value)) {
            complete = 0;
        } else {
            reply->data.byte[i] = value;
        }
    }
    if (!complete) {
        *reply = (zg335_read_reply){0};
        reply->state32 = ZG335_SERVICE_STOPPED;
        reply->reason32 = 0x7001U;
        return;
    }
    reply->state32 = ZG335_SERVICE_OK;
    reply->initialized_mask32 = width == 16 ? 0xffffU : ((1U << width) - 1U);
}

static void probe_write(void *user, uint32_t space, uint32_t address,
    uint32_t width, zg335_block16 data, zg335_write_reply *reply)
{
    Probe *p = (Probe *)user;
    uint32_t i;
    *reply = (zg335_write_reply){0};
    for (i = 0; i < width; ++i) {
        if (space == ZG335_SPACE_LOGICAL_FS) {
            uint32_t shift = (address + i) * 8U;
            if (address + i < 4) {
                p->fs_head = (p->fs_head & ~(0xffU << shift)) |
                    ((uint32_t)data.byte[i] << shift);
                p->fs_initialized = 1;
            }
        } else {
            put8(p, address + i, data.byte[i]);
        }
    }
    reply->state32 = ZG335_SERVICE_OK;
    reply->committed_prefix32 = width;
}

static zg116_xmm0 changed_xmm(zg116_xmm0 old, uint32_t ordinal)
{
    uint32_t n = ordinal + 1U;
    zg116_xmm0 out;
    out.w0 = old.w0 + 0x00010101U * n;
    out.w1 = old.w1 + 0x00020203U * n;
    out.w2 = old.w2 + 0x00030407U * n;
    out.w3 = old.w3 + 0x0004080fU * n;
    return out;
}

static void probe_machine(void *user, zg116la_machine_call request,
    zg116la_machine_reply *reply)
{
    Probe *p = (Probe *)user;
    const uint32_t ordinal = ++p->machine_count;
    *reply = (zg116la_machine_reply){0};
    reply->state32 = ZG116_SERVICE_OK;
    reply->normal_abi_preserved = 1;
    reply->regs_present = 1;
    reply->returned = request.entry;
    reply->xmm0_present = 1;
    reply->returned_to_present = 1;
    reply->returned_to32 = request.continuation32;
    if (request.seam32 == ZG116LA_SEAM_ALLOCATE) {
        uint32_t node = p->allocate_index < p->allocated_count ?
            p->allocated[p->allocate_index] : 0;
        ++p->allocate_index;
        reply->returned.eax = node;
        reply->returned.esp = request.entry.esp + 4U;
    } else if (request.seam32 == ZG116LA_SEAM_FINISH_NORMAL) {
        reply->returned.eax = request.core_return_value32;
        reply->returned.esp = request.entry.esp + 4U + request.callee_ret_cleanup32;
        reply->returned_to32 = peek32(p, request.entry.esp);
    } else if (request.seam32 == ZG116LA_SEAM_SLOT4 ||
        request.seam32 == ZG116LA_SEAM_SLOT8) {
        reply->returned_to32 = 0x90000000U + request.seam32;
    } else if (request.seam32 == ZG116LA_SEAM_LENGTH_ERROR) {
        reply->returned.esp = request.entry.esp + 4U;
    }
    reply->xmm0_returned = changed_xmm(p->current_xmm, ordinal);
    p->current_xmm = reply->xmm0_returned;
}

static void setup(Probe *p, zg335_entry *entry, int nonempty)
{
    uint32_t S = 0x00010000U;
    uint32_t D = 0x00020000U;
    uint32_t B = 0x00030000U;
    uint32_t T = 0x6a5b4c3dU;
    uint32_t F = 0x89abcdefU;
    uint32_t i;

    entry->regs_present = 1;
    entry->regs.eax = 0x11111111U;
    entry->regs.ecx = D;
    entry->regs.edx = 0x33333333U;
    entry->regs.ebx = 0x44444444U;
    entry->regs.esp = S;
    entry->regs.ebp = 0x66666666U;
    entry->regs.esi = 0x77777777U;
    entry->regs.edi = 0x88888888U;
    entry->xmm0_present = 1;
    entry->xmm0.w0 = 0x01020304U;
    entry->xmm0.w1 = 0x11121314U;
    entry->xmm0.w2 = 0x21222324U;
    entry->xmm0.w3 = 0x31323334U;
    p->current_xmm.w0 = entry->xmm0.w0;
    p->current_xmm.w1 = entry->xmm0.w1;
    p->current_xmm.w2 = entry->xmm0.w2;
    p->current_xmm.w3 = entry->xmm0.w3;
    p->fs_head = F;
    p->fs_initialized = 1;

    put32(p, S, T);
    put32(p, S + 4U, B);
    for (i = 0; i < 9; ++i) {
        uint32_t Q = 0x00040000U + i * 0x1000U;
        uint32_t C = Q + 0x100U;
        uint32_t N0 = 0x00060000U + i * 0x1000U;
        uint32_t N1 = N0 + 0x100U;
        uint32_t x = 4U + 8U * i;
        put32(p, B + x, Q);
        if (nonempty) {
            put32(p, Q, C);
            p->allocated[p->allocated_count++] = N0;
            p->allocated[p->allocated_count++] = N1;
            put32(p, C, Q);
            {
                uint32_t offset;
                for (offset = 0; offset < 36; offset += 4) {
                    put32(p, N1 + offset, 0);
                }
            }
            put32(p, C + 8U, 0xabc00000U + i);
            put32(p, C + 12U, 0);
            put32(p, C + 16U, 0x88888888U);
            put32(p, C + 20U, 0);
            put32(p, C + 24U, 0x16161616U);
            put32(p, C + 28U, 0x20202020U);
            put32(p, C + 32U, 0);
        } else {
            put32(p, Q, Q);
            p->allocated[p->allocated_count++] = N0;
        }
    }
    {
        const uint32_t offsets[8] = {0x48, 0x58, 0x64, 0x74, 0x80, 0x90, 0x9c, 0xac};
        const uint32_t widths[8] = {16, 8, 16, 8, 16, 8, 16, 8};
        uint32_t j;
        for (j = 0; j < 8; ++j) {
            uint32_t k;
            for (k = 0; k < widths[j]; k += 4) {
                put32(p, B + offsets[j] + k,
                    0x51000000U + j * 0x01000000U + k * 0x00010101U);
            }
        }
    }
}

int main(void)
{
    static Probe probe;
    int failed = 0;
    int nonempty;

    for (nonempty = 0; nonempty < 2; ++nonempty) {
        zg335_entry entry;
        zg335116_config configuration;
        zg335116_output output;
        zg116la_config machine;
        uint32_t state;
        uint32_t i;
        int ok;

        memset(&probe, 0, sizeof probe);
        memset(&entry, 0, sizeof entry);
        setup(&probe, &entry, nonempty);

        memset(&configuration, 0, sizeof configuration);
        configuration.version32 = ZG335116_VERSION;
        configuration.memory.user = &probe;
        configuration.memory.read = probe_read;
        configuration.memory.write = probe_write;
        machine.version32 = ZG116LA_VERSION;
        machine.machine_user = &probe;
        machine.machine_call = probe_machine;
        if (!zg116la_bind_rich335(&configuration, &machine)) {
            printf("behavior bind failed mode=%s\n", nonempty ? "nonempty" : "empty");
            return 2;
        }

        memset(&output, 0, sizeof output);
        state = zg335116_run(entry, &configuration, &output);
        printf("behavior mode=%s state=%u raw_state=%u reason=%u dispatched=%u "
            "continued=%u normal=%u overflow=%u eax=%08x\n",
            nonempty ? "nonempty" : "empty",
            (unsigned)state,
            (unsigned)output.raw_result.state32,
            (unsigned)output.raw_result.reason32,
            (unsigned)output.raw_result.calls_dispatched32,
            (unsigned)output.raw_result.calls_continued32,
            (unsigned)output.normal_count32,
            (unsigned)output.record_overflow32,
            (unsigned)output.raw_result.regs.eax);
        for (i = 0; i < output.normal_count32 && i < ZG335116_NORMAL_RECORD_SLOTS; ++i) {
            printf("normal[%u] state=%u reason=%u\n", (unsigned)i,
                (unsigned)output.normal_records[i].normal_result.state32,
                (unsigned)output.normal_records[i].normal_result.reason32);
        }

        ok = state == ZG335116_HOST_COMPLETED &&
            output.raw_result.state32 == ZG335_RETURNED &&
            output.raw_result.calls_dispatched32 == 8U &&
            output.raw_result.calls_continued32 == 8U &&
            output.normal_count32 == 8U &&
            output.record_overflow32 == 0U;
        for (i = 0; i < 8U; ++i) {
            if (i >= output.normal_count32 ||
                output.normal_records[i].normal_result.state32 != ZG116_RETURNED) {
                ok = 0;
            }
        }
        printf("behavior mode=%s RESULT %s\n", nonempty ? "nonempty" : "empty",
            ok ? "PASS" : "FAIL");
        if (!ok) {
            failed = 1;
        }
    }
    return failed;
}
