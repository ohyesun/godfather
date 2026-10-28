#include <stdio.h>
#include <string.h>

#include "zg335_portable.h"

int main(void)
{
    zg335_entry entry;
    zg335116_config configuration;
    zg335116_output output;
    uint32_t state;

    memset(&entry, 0, sizeof entry);
    memset(&configuration, 0, sizeof configuration);
    memset(&output, 0, sizeof output);
    state = zg335116_run(entry, &configuration, &output);
    printf("consumer state=%u normal=%u overflow=%u\n",
        (unsigned)state, (unsigned)output.normal_count32,
        (unsigned)output.record_overflow32);
    return state == ZG335116_HOST_BAD_INTERFACE ? 0 : 1;
}
