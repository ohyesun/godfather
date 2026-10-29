# Android Host Smoke

Standalone Android host APK for the `zg335_portable` static library.

## Toolchain

- Gradle 8.6
- AGP 8.1.2
- compileSdk 34, minSdk 24, targetSdk 34
- Java only; namespace/applicationId `com.zg335.host`
- NDK r27 (`27.0.12077973`)
- ABIs: `x86_64`, `arm64-v8a`

## Build

```bash
export JAVA_HOME="C:/Program Files/Microsoft/jdk-17.0.16.8-hotspot"
export ANDROID_HOME="D:/Android/sdk"
export ANDROID_SDK_ROOT="D:/Android/sdk"
./gradlew assembleDebug
```

The prebuilt libraries are project-relative copies:

```text
prebuilt/x86_64/lib/libzg335_portable.a
prebuilt/arm64-v8a/lib/libzg335_portable.a
prebuilt/include/zg335/*.h
```

`app/src/main/cpp/CMakeLists.txt` selects
`prebuilt/${ANDROID_ABI}/lib/libzg335_portable.a`; no absolute library path is
embedded.

## APK

- Path: `app/build/outputs/apk/debug/app-debug.apk`
- Size: `59077` bytes
- SHA-256: `abeea134853088491a0db15fcf50c69f2dcfb34366b6426dbe1e4b8cb6c6d6c6`
- Native libraries:
  - `lib/x86_64/libzg335host.so` (23328 bytes)
  - `lib/arm64-v8a/libzg335host.so` (20896 bytes)

Both shared libraries export:

```text
T Java_com_zg335_host_MainActivity_runProbe
```

## Install and launch

```text
adb install -r app/build/outputs/apk/debug/app-debug.apk
Success
adb shell am start -n com.zg335.host/.MainActivity
Starting: Intent { cmp=com.zg335.host/.MainActivity }
```

The Activity is top-resumed and `dumpsys package` reports
`primaryCpuAbi=x86_64`, `minSdk=24`, `targetSdk=34`.

## Native probe

`onCreate` invokes `runProbe()` and the button invokes it again:

```text
state=1 normal=0 overflow=0
```

Captured logcat:

```text
I ZG335HOST: state=1 normal=0 overflow=0
```

Tapping the `RUN ZG335 PROBE` button logs the same line again.

`state=1` is `ZG335116_HOST_BAD_INTERFACE`, expected for the deliberately empty
zero-initialized native entry/config.

## Project structure

```text
android-host-smoke/
  settings.gradle
  build.gradle
  gradle.properties
  gradlew / gradlew.bat
  gradle/wrapper/
  app/
    build.gradle
    src/main/AndroidManifest.xml
    src/main/java/com/zg335/host/MainActivity.java
    src/main/cpp/CMakeLists.txt
    src/main/cpp/jni_bridge.c
  prebuilt/
    include/zg335/*.h
    x86_64/lib/libzg335_portable.a
    arm64-v8a/lib/libzg335_portable.a
```

`local.properties` is machine-local SDK configuration and is excluded by
`.gitignore`; it is not a source dependency.

## Scope

This verifies APK installation, Activity lifecycle, JNI loading, and linkage to
the static library on the local x86_64 emulator. It does not claim production
runtime, physical ARM64, signing, store distribution, SDK/OS/ABI or whole-game
acceptance.
