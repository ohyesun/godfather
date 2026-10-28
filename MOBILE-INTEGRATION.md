# zg335_portable — mobile host integration

Artifacts live in `mobile-local-artifacts-20261028/`.

## Targets

| Target | Directory | Library |
|---|---|---|
| Android x86_64 | `android-x86_64/` | `lib/libzg335_portable.a` |
| Android arm64-v8a | `android-arm64/` | `lib/libzg335_portable.a` |
| iOS device arm64 | `ios-device-arm64/` | `lib/libzg335_portable.a` |

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
