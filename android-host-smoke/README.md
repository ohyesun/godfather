# Android Host Smoke

Installable Android host for `zg335_portable`, now also used as the WebView UI
baseline for the mobile rewrite.

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

Import the legacy WebView assets first; they are intentionally not committed:

```bash
bash tools/import-legacy-assets.sh
# default: D:/zg/apk114-bridge/output/apk114-minimal-r129b2-signed.apk
# or:     bash tools/import-legacy-assets.sh /path/to/old.apk
```

This copies all `assets/` entries (1,783 files, ~85 MB) into
`app/src/main/assets/`. The directory is git-ignored.

## Current APK

- Path: `app/build/outputs/apk/debug/app-debug.apk`
- Size: `42 MB` with legacy assets
- SHA-256: `392da36647e84d45a888e40ede04aa78090e4cb3d6d6a3beeb53520221448c46`
- Native libraries:
  - `lib/x86_64/libzg335host.so`
  - `lib/arm64-v8a/libzg335host.so`
- JNI export: `Java_com_zg335_host_MainActivity_runProbe`

## WebView baseline

`MainActivity` loads `file:///android_asset/index.html` with JavaScript, DOM
storage, file URL access, mixed content and hardware acceleration enabled. The
legacy Three.js UI renders the game scene and HUD on MuMu; the native probe
still logs:

```text
I ZG335HOST: state=1 normal=0 overflow=0
```

The UI is reused. The legacy **data path is not**: see
`ANDROID-WEBVIEW-BRIDGE.md`.

## Scope

This verifies APK installation, Activity lifecycle, JNI loading, static library
linkage, and that the legacy WebView UI can render on the new host. It does not
claim production runtime, physical ARM64, signing, store distribution,
SDK/OS/ABI or whole-game acceptance.
