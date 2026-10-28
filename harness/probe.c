#include "zg335_portable.h"

#include <stdio.h>

int main(void)
{
    zg335116_config configuration = {0};
    zg335_entry entry = {0};
    zg335116_output output = {0};
    uint32_t state = zg335116_run(entry, &configuration, &output);

    printf("zg335_probe state=%u raw_present=%u normal_count=%u overflow=%u\n",
        (unsigned)state,
        (unsigned)output.raw_result_present,
        (unsigned)output.normal_count32,
        (unsigned)output.record_overflow32);
    printf("zg335_probe bind_null=%d\n",
        zg116la_bind_rich335(0, 0));
    return 0;
}
