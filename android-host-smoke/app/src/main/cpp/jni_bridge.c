#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zg335_portable.h"
#include "zg_bridge.h"

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

JNIEXPORT jlong JNICALL
Java_com_zg335_host_ZgBridge_nativeCreate(JNIEnv *env, jclass clazz,
    jint abi, jint max_frame, jint max_packet, jint max_tx, jint flags)
{
    zg_bridge_config config;
    zg_bridge *bridge;

    (void)env;
    (void)clazz;
    memset(&config, 0, sizeof config);
    config.abi_version = (uint32_t)abi;
    config.max_frame_bytes = (uint32_t)max_frame;
    config.max_packet_bytes = (uint32_t)max_packet;
    config.max_tx_bytes = (uint32_t)max_tx;
    config.flags = (uint32_t)flags;
    bridge = zg_bridge_create(&config);
    return (jlong)(intptr_t)bridge;
}

JNIEXPORT jint JNICALL
Java_com_zg335_host_ZgBridge_nativeFeed(JNIEnv *env, jclass clazz,
    jlong handle, jobject source, jint length, jlong now_ms)
{
    zg_bridge *bridge = (zg_bridge *)(intptr_t)handle;
    void *address;
    jlong capacity;

    (void)clazz;
    if (bridge == NULL || source == NULL || length < 0) {
        return (jint)ZG_BRIDGE_PROTOCOL_ERROR;
    }
    address = (*env)->GetDirectBufferAddress(env, source);
    capacity = (*env)->GetDirectBufferCapacity(env, source);
    if (address == NULL || capacity < 0 || (jlong)length > capacity) {
        return (jint)ZG_BRIDGE_PROTOCOL_ERROR;
    }
    return (jint)zg_bridge_feed(bridge, (const uint8_t *)address,
        (size_t)length, (uint64_t)now_ms);
}

JNIEXPORT jint JNICALL
Java_com_zg335_host_ZgBridge_nativeStats(JNIEnv *env, jclass clazz,
    jlong handle, jintArray out, jint capacity)
{
    zg_bridge *bridge = (zg_bridge *)(intptr_t)handle;
    zg_bridge_stats stats;
    jint values[3];

    (void)clazz;
    if (bridge == NULL || out == NULL || capacity < 3) {
        return 0;
    }
    zg_bridge_get_stats(bridge, &stats);
    values[0] = (jint)(uint32_t)stats.rx_bytes;
    values[1] = (jint)(uint32_t)stats.frames;
    values[2] = (jint)stats.last_error;
    (*env)->SetIntArrayRegion(env, out, 0, 3, values);
    return 3;
}

JNIEXPORT jint JNICALL
Java_com_zg335_host_ZgBridge_nativeRequestFull(JNIEnv *env, jclass clazz,
    jlong handle)
{
    zg_bridge *bridge = (zg_bridge *)(intptr_t)handle;

    (void)env;
    (void)clazz;
    if (bridge == NULL) {
        return (jint)ZG_BRIDGE_PROTOCOL_ERROR;
    }
    return (jint)zg_bridge_request_full(bridge);
}

JNIEXPORT jint JNICALL
Java_com_zg335_host_ZgBridge_nativePollRender(JNIEnv *env, jclass clazz,
    jlong handle, jobject destination, jint capacity)
{
    zg_bridge *bridge = (zg_bridge *)(intptr_t)handle;
    void *address;
    jlong buffer_capacity;
    size_t out_length = 0;
    zg_bridge_status status;

    (void)clazz;
    if (bridge == NULL || destination == NULL || capacity < 0) {
        return (jint)ZG_BRIDGE_PROTOCOL_ERROR;
    }
    address = (*env)->GetDirectBufferAddress(env, destination);
    buffer_capacity = (*env)->GetDirectBufferCapacity(env, destination);
    if (address == NULL || buffer_capacity < 0 ||
        (jlong)capacity > buffer_capacity) {
        return (jint)ZG_BRIDGE_OVERSIZE;
    }
    status = zg_bridge_poll_render(bridge, (uint8_t *)address,
        (size_t)capacity, &out_length);
    if (status == ZG_BRIDGE_OK) {
        return (jint)out_length;
    }
    return (jint)status;
}

JNIEXPORT void JNICALL
Java_com_zg335_host_ZgBridge_nativeDestroy(JNIEnv *env, jclass clazz,
    jlong handle)
{
    (void)env;
    (void)clazz;
    zg_bridge_destroy((zg_bridge *)(intptr_t)handle);
}

