#ifndef ZG_COPY335_RICH116_V1_H
#define ZG_COPY335_RICH116_V1_H

/* New typed composition interface. Frozen component declarations are included
 * unchanged. These are host control types, not a guest/native ABI or wire
 * layout; padding and function/user pointer bits are not serialized state.
 */
#ifdef __cplusplus
extern "C" {
#endif
#include "zg_copy335_raw_normal_frame.h"
#include "zg_normal116_fullstate.h"

#define ZG335116_VERSION UINT32_C(1)
#define ZG335116_NORMAL_RECORD_SLOTS UINT32_C(8)
#define ZG335116_HOST_UNSET UINT32_C(0)
#define ZG335116_HOST_BAD_INTERFACE UINT32_C(1)
#define ZG335116_HOST_COMPLETED UINT32_C(2)
#define ZG335116_HOST_CAPTURE_INCOMPLETE UINT32_C(3)

/* All four functions are required. Each user may be null only if that
 * particular provider permits it. Whole configuration selection is copied
 * once per zg335116_run; guest bytes and pointed-to userdata are not cloned.
 * Reads must preserve guest bytes/init/alias maps for their entire dynamic
 * extent. Write/inner nested effects follow the ROOT-selected domain.
 */
typedef struct zg335116_config {
    uint32_t version32;
    zg335_memory memory;
    void *e4c0_sentinel_user;
    zg116_lower_fn e4c0_sentinel;
    void *e4c0_range_user;
    zg116_lower_fn e4c0_range;
    void *fe0_sentinel_user;
    zg116_lower_fn fe0_sentinel;
    void *fe0_range_user;
    zg116_lower_fn fe0_range;
} zg335116_config;

typedef struct zg335116_normal_record {
    zg335_call request;                 /* actual post-raw-CALL request */
    zg116_result normal_result;         /* all24 members, including raw replies */
    zg335_lower_reply projected_reply;  /* actual10-member projection */
} zg335116_normal_record;

typedef struct zg335116_output {
    uint32_t raw_result_present;
    zg335_result raw_result;            /* independent full22-member result */
    uint32_t normal_count32;            /* retained complete records,0..8 */
    uint32_t record_overflow32;         /* capture incomplete, never guest stop */
    zg335116_normal_record normal_records[ZG335116_NORMAL_RECORD_SLOTS];
} zg335116_output;

/* Host status is separate from every guest result state. A null output returns
 * BAD_INTERFACE without dereferencing output or configuration. An ordinary
 * nonnull output initializes all logical members to0. Invalid configuration
 * returns BAD_INTERFACE before raw execution, leaving it allzero. A valid
 * configuration always retains the raw core's own result, including BAD_ENTRY.
 *
 * Caller exclusively owns output, separate from configuration, provider control
 * and guest backing. Synchronous nested composition uses a distinct output.
 * Providers must not retain private request/services/bridge addresses. No
 * hostile callback, longjmp, concurrency, fault or process-wide state guarantee.
 * Slots are post-call records, not an execution observer or full event history.
 */
uint32_t zg335116_run(zg335_entry entry,
    const zg335116_config *configuration, zg335116_output *output);

#ifdef __cplusplus
}
#endif
#endif
