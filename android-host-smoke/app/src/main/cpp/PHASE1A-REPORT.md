# JNI ByteBuffer Phase 1A Report

Task: T40. Scope: bind Phase 0 C bridge into the existing Android APK; prove a
direct ByteBuffer feed/stats path. No TLS/network, no game fields, no assets or
old Java changed.

## Files

- `app/src/main/java/com/zg335/host/ZgBridge.java`
  - `loadLibrary("zg335host")`
  - native `nativeCreate`, `nativeFeed`, `nativeStats`, `nativeDestroy`
  - Java `create/feed/stats/destroy/close` wrappers
- `app/src/main/cpp/jni_bridge.c`
  - existing `runProbe`
  - new `Java_com_zg335_host_ZgBridge_nativeCreate`
  - new `Java_com_zg335_host_ZgBridge_nativeFeed`
  - new `Java_com_zg335_host_ZgBridge_nativeStats`
  - new `Java_com_zg335_host_ZgBridge_nativeDestroy`
- `app/src/main/cpp/CMakeLists.txt`
  - adds `${ZG_ROOT}/bridge/src/zg_bridge.c`
  - includes `${ZG_ROOT}/bridge/include`
  - keeps `prebuilt/${ANDROID_ABI}/lib/libzg335_portable.a`
- `app/src/main/java/com/zg335/host/MainActivity.java`
  - creates `ZgBridge` in `onCreate`
  - builds a 24-byte little-endian CONTROL envelope:
    magic `0x5A47`, version 1, kind 3, header_bytes 24, frame_id 1,
    payload 0, record_count 0
  - feeds via direct `ByteBuffer`, reads stats, logs `ZG335BRIDGE`
  - destroys in `onDestroy`

## Build and install

```text
./gradlew assembleDebug
BUILD SUCCESSFUL

adb install -r app/build/outputs/apk/debug/app-debug.apk
Success
```

APK:

- Path: `app/build/outputs/apk/debug/app-debug.apk`
- Size: 43,979,980 bytes
- SHA-256: `6ad9b8b3073183d5c33abd978aec1de883eb6ae646f99dce870c1562039d170d`

Native libraries in APK:

```text
lib/arm64-v8a/libzg335host.so  26176
lib/x86_64/libzg335host.so     28320
```

Both export:

```text
T Java_com_zg335_host_MainActivity_runProbe
T Java_com_zg335_host_ZgBridge_nativeCreate
T Java_com_zg335_host_ZgBridge_nativeFeed
T Java_com_zg335_host_ZgBridge_nativeStats
T Java_com_zg335_host_ZgBridge_nativeDestroy
```

## Device log

Device: `emulator-5554`, x86_64.

```text
I ZG335HOST: state=1 normal=0 overflow=0
I ZG335BRIDGE: status=0 rx=24 frames=1 error=0
```

Expected values:

- feed status `0` = `ZG_BRIDGE_OK`
- `rx=24` = one 24-byte control envelope accepted
- `frames=1` = one accepted feed chunk in Phase 0 semantics
- `error=0` = no bridge error

Process remained alive (`pidof com.zg335.host` non-empty).

## Boundary

This proves the JNI direct-ByteBuffer path and C bridge linkage. It does not
connect TLS, parse game commands, decode frame payloads, build render packets,
or modify `client.js`/assets/old Java/app protocol code. No push was performed.
