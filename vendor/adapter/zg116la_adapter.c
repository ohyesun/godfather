#include "zg116la_adapter.h"

static int zg116la_machine_valid(const zg116la_config *machine)
{
    return machine != 0 && machine->version32 == ZG116LA_VERSION &&
        machine->machine_call != 0;
}

int zg116la_bind_normal116(zg116_services *services,
    const zg116la_config *machine, uint32_t wrapper_entry32)
{
    zg116_services out;
    if (services == 0 || !zg116la_machine_valid(machine) ||
        (wrapper_entry32 != ZG116_ENTRY_7FE0 &&
            wrapper_entry32 != ZG116_ENTRY_E4C0)) { return 0; }
    out = *services;
    out.version32 = ZG116_VERSION;
    out.sentinel_user = (void *)machine;
    out.range_user = (void *)machine;
    if (wrapper_entry32 == ZG116_ENTRY_7FE0) {
        out.sentinel = zg116la_fe0_sentinel;
        out.range = zg116la_fe0_range;
    } else {
        out.sentinel = zg116la_e4c0_sentinel;
        out.range = zg116la_e4_range;
    }
    *services = out;
    return 1;
}

int zg116la_bind_rich335(zg335116_config *services,
    const zg116la_config *machine)
{
    zg335116_config out;
    if (services == 0 || !zg116la_machine_valid(machine)) { return 0; }
    out = *services;
    out.version32 = ZG335116_VERSION;
    out.e4c0_sentinel_user = (void *)machine;
    out.e4c0_sentinel = zg116la_e4c0_sentinel;
    out.e4c0_range_user = (void *)machine;
    out.e4c0_range = zg116la_e4_range;
    out.fe0_sentinel_user = (void *)machine;
    out.fe0_sentinel = zg116la_fe0_sentinel;
    out.fe0_range_user = (void *)machine;
    out.fe0_range = zg116la_fe0_range;
    *services = out;
    return 1;
}
