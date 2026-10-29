# zg335_portable — mobile host integrationhttps://codemagic.io/app/6ae1e7a0de99a014ebac5db6



Artifacts live in `mobile-local-artifacts-20261028/`.

## Targets

| Target            | Directory           | Library                   |
| ----------------- | ------------------- | ------------------------- |
| Android x86_64    | `android-x86_64/`   | `lib/libzg335_portable.a` |
| Android arm64-v8a | `android-arm64/`    | `lib/libzg335_portable.a` |
| iOS device arm64  | `ios-device-arm64/` | `lib/libzg335_portable.a` |

Every target ships `include/zg335/` (14 headers) and `lib/cmake/zg335/`.

## Android

With the installed CMake package:

```cmake
find_package(zg335 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE zg335::zg335_portable)
```

Direct NDK link:

```bash
$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/clang \
  --target=aarch64-linux-android24 \
  --sysroot=$NDK/toolchains/llvm/prebuilt/windows-x86_64/sysroot \
  -Iandroid-arm64/include/zg335 \
  your.c android-arm64/lib/libzg335_portable.a -o your_binary
```

### Consumer smoke

`consumer-smoke/` is a minimal CMake project that uses the installed package:

```cmake
find_package(zg335 CONFIG REQUIRED)
target_link_libraries(zg335_consumer PRIVATE zg335::zg335_portable)
```

Configure with the installed package directory, because the Android NDK  
toolchain does not inherit an outer `CMAKE_PREFIX_PATH`:

```bash
cmake -S consumer-smoke -B build-cons-arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -Dzg335_DIR=$ARTIFACTS/android-arm64/lib/cmake/zg335
cmake --build build-cons-arm64
```

Both ABIs linked. The x86_64 binary ran on MuMu:

```text
consumer state=1 normal=0 overflow=0
EXIT=0
```

`state=1` is `BAD_INTERFACE` for a zeroed configuration, as expected.

### Android host APK smoke

`android-host-smoke/` is an installable APK, not a CLI consumer:

- Java `MainActivity` loads `libzg335host.so` and calls the JNI bridge
- `jni_bridge.c` calls `zg335116_run` with zeroed configuration
- `prebuilt/` carries x86_64 and arm64-v8a static libraries
- Gradle 8.6 + AGP 8.1.2, compileSdk 34, minSdk 24

Verified on MuMu:

```text
adb install -r app-debug.apk            -> Success
topResumedActivity=com.zg335.host/.MainActivity
I ZG335HOST: state=1 normal=0 overflow=0
```

APK: `abeea134853088491a0db15fcf50c69f2dcfb34366b6426dbe1e4b8cb6c6d6c6c`
(59,077 B). The button and `onCreate` both produce the same line.

## iOS

Add `ios-device-arm64/lib/libzg335_portable.a` to the Xcode target and add  
`ios-device-arm64/include` to Header Search Paths. The archive is arm64 device  
only; there is no simulator slice.

The library exports these defined symbols (verified with `llvm-nm`):

```text
T _zg116_normal_frame
T _zg335116_run
T _zg335_raw_normal_frame
T _zg116la_bind_rich335
```

The host must supply the memory and lower/machine callbacks described by the  
headers. The library does not provide guest memory, filesystem access, or a  
UI. Code signing, entitlements and IPA packaging belong to the host app.

### Apple link smoke

GitHub Actions workflow `.github/workflows/ios-link-smoke.yml` builds the iOS
library on `macos-14`, then links `consumer-smoke/main.c` with
`xcrun --sdk iphoneos clang -arch arm64` against the installed static library.

Run `40814089191` on HEAD `dab5873` completed with every step `success`.
The `Record library identity` step asserts:

```text
git diff --exit-code 4d7c66f -- design vendor
ios_source_closure = matches 4d7c66f
```

So the current HEAD has the same C source closure as the Codemagic artifact
commit. The HEAD rebuild library hash is
`4a51a3c8848345c88552922c9163ba7fc886590177ba8914e18c53e48076d31b`
(different from the Codemagic archive hash `d839e3bd...` because the Xcode
toolchain and archive metadata differ; the source closure is identical).
`Link consumer` and `Upload link-smoke artifact` also succeeded.

An earlier run `40811767100` on `228ef5f` also passed the Apple link and
uploaded a 65,509 B artifact (digest
`sha256:a2b39f2a50b14662bcde47235d302e753643972b0187b0e264d6dd52b2a70aa0`).

This proves the library is consumable by the Apple toolchain; it does not run
the binary on an iOS device.

## Evidence

- Android behavior gate: 8 scenarios, both ABIs, PASS; QA report  
  `c968a87c...`
- iOS build: Codemagic `6ae1fdb1206ce93970c6dddb`, commit `4d7c66f`,  
  10m28s, all steps green
- iOS library: 13 Mach-O arm64 objects, 0 ELF, 73,488 B,  
  SHA-256 `d839e3bd...`
- iOS artifact zip: 58,224 B, SHA-256 `40bd0c34...`
- iOS QA: PASS for payload/hash/architecture/non-Android;  
  report `3f7f1d8b...`

## Boundaries

These are static artifacts and local build/harness evidence only. They do not  
grant iOS runtime, signing/IPA, physical ARM64 device, production runtime,  
SDK/OS/native ABI or whole-game acceptance.
