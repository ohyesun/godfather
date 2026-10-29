#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zg335_portable.h"

JNIEXPORT jstring JNICALL
Java_com_zg335_host_MainActivity_runProbe(JNIEnv *env, jobject thiz)
{
    zg335_entry entry;
    zg335116_config configuration;
    zg335116_output output;
    uint32_t state;
    char text[128];

    (void)thiz;
    memset(&entry, 0, sizeof entry);
    memset(&configuration, 0, sizeof configuration);
    memset(&output, 0, sizeof output);

    state = zg335116_run(entry, &configuration, &output);
    snprintf(text, sizeof text, "state=%u normal=%u overflow=%u",
        (unsigned)state,
        (unsigned)output.normal_count32,
        (unsigned)output.record_overflow32);
    return (*env)->NewStringUTF(env, text);
}
