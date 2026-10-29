# Android Host Smoke Verification

Date: 2026-10-29
Device: `emulator-5554`, x86_64, Android 12 (MuMu-compatible local emulator)

## Build

```text
Gradle 8.6
AGP 8.1.2
compileSdk 34 / minSdk 24 / targetSdk 34
NDK 27.0.12077973
./gradlew assembleDebug
BUILD SUCCESSFUL
```

## APK

```text
app/build/outputs/apk/debug/app-debug.apk
59077 bytes
SHA-256 abeea134853088491a0db15fcf50c69f2dcfb34366b6426dbe1e4b8cb6c6d6c6
lib/arm64-v8a/libzg335host.so 20896 bytes
lib/x86_64/libzg335host.so   23328 bytes
```

Both ABI libraries export:

```text
T Java_com_zg335_host_MainActivity_runProbe
```

## Install / launch

```text
adb install -r app-debug.apk
Success

adb shell am start -n com.zg335.host/.MainActivity
Starting: Intent { cmp=com.zg335.host/.MainActivity }
```

Activity state:

```text
topResumedActivity=ActivityRecord{... com.zg335.host/.MainActivity}
primaryCpuAbi=x86_64
versionCode=1 minSdk=24 targetSdk=34
```

## Native result

Automatic onCreate run and button tap both produce:

```text
I ZG335HOST: state=1 normal=0 overflow=0
```

`state=1` is `ZG335116_HOST_BAD_INTERFACE` for the intentionally empty
zero-initialized entry/config.

## Boundary

This is a local emulator APK/JNI/lifecycle smoke test. It does not grant
production runtime, physical ARM64, signing, store distribution, SDK/OS/native
ABI or whole-game acceptance.
