#ifndef ZG116LA_ADAPTER_H
#define ZG116LA_ADAPTER_H

#include "zg116la_raw52.h"
#include "zg116la_fe0_range.h"
#include "zg116la_e4_range.h"
#include "zg_copy335_rich116.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Validate the shared machine configuration, then bind the reviewed lower
 * callbacks. Existing outer memory/user fields are preserved. The machine
 * object must remain alive and unmodified for every later invocation. No guest
 * or native call occurs. normal116 requires an explicit FE0/E4C0 entry.
 */
int zg116la_bind_normal116(zg116_services *services,
    const zg116la_config *machine, uint32_t wrapper_entry32);
int zg116la_bind_rich335(zg335116_config *services,
    const zg116la_config *machine);

#ifdef __cplusplus
}
#endif
#endif
