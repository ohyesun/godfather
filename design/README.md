# Android / iOS Portable Integration Design

Status: design only, not built, not SDK- or runtime-accepted.
Baseline: F2/F3 source closure plus F4 limited structural SOURCE acceptance.
Do not inherit Host, runtime, SDK, OS, native ABI, CRT, SEH, atomicity, allocation-ownership, composed26, or whole-game claims.

## 1. Objective

Produce one C11 static-library layout that can be consumed by Android arm64-v8a/x86_64 and iOS arm64 hosts without any fixture macro redirection, test wrapper, or test-only hook.

The production library must expose the reviewed host-control types and entry points, while the platform host supplies the real provider callbacks and guest backing memory.

## 2. Production source manifest

Thirteen real C translation units are compiled, unmodified:

| Role | Translation unit |
|---|---|
| normal116 fullstate | `zg_normal116_fullstate.c` |
| node provider | `zg_node_provider52_candidate.c` |
| node copy | `zg_node_copy32_candidate.c` |
| portable wrapper | `zg_wrapper154_candidate.c` |
| range76e0 | `zg_range76e0.c` |
| range101e0 raw frame | `zg_range101e0_raw_frame.c` |
| rich116 composition | `zg_copy335_rich116.c` |
| raw/normal frame | `zg_copy335_raw_normal_frame.c` |
| adapter common | `zg116la_common.c` |
| adapter raw52 | `zg116la_raw52.c` |
| adapter FE0 range | `zg116la_fe0_range.c` |
| adapter E4 range | `zg116la_e4_range.c` |
| adapter bindings | `zg116la_adapter.c` |

The exact paths and SHA-256 pins are in `SOURCE-MANIFEST.json`.

Two source roots are required and must not be conflated:

- `ZG_LEGACY_ROOT` = `D:/zg/REA研究/zg-dat-20261017/animation-contract-20261021`
- `ZG_ADAPTER_ROOT` = `E:/zg-handoff-workbuddy/normal116-four-lower-reuse-adapter-C11-author-20261028-v1`

Excluded from the production library:

- `fixture.cpp`
- `fixture_check.py`
- all `F4-*`, `F3-*`, `F2-*` reports and QA files
- fixture-only source-specific macro definitions such as `zg116_normal_frame=fixture_rich_normal_adapter`
- test oracles, hook recorders, controlled-overflow wrappers, and observed-trace generators

## 3. Public integration surface

The initial consumer API is the existing reviewed surface:

- `zg335116_run(zg335_entry, const zg335116_config *, zg335116_output *)`
- `zg116la_bind_normal116(...)`
- `zg116la_bind_rich335(...)`
- host-control types from `zg_copy335_rich116.h` and `zg116la_adapter.h`

Recommended installed include layout:

```text
include/zg335/
  zg335_portable.h       # generated umbrella, no new ABI
  zg116la_adapter.h
  zg116la_common.h
  zg116la_raw52.h
  zg116la_fe0_range.h
  zg116la_e4_range.h
  zg_copy335_rich116.h
  zg_copy335_raw_normal_frame.h
  zg_normal116_fullstate.h
  zg_node_provider52_candidate.h
  zg_node_copy32_candidate.h
  zg_wrapper154_candidate.h
  zg_range76e0.h
  zg_range101e0_raw_frame.h
```

The generated umbrella includes only reviewed headers and declares no new ABI. Do not put host pointers or platform-specific types into guest-visible state.

`include/zg335/` contains byte copies of all thirteen reviewed headers, including the five transitive lower/provider headers required by `zg116la_adapter.h`, plus the generated umbrella. The reviewed copies are packaging copies, not a new API; keep them hash-identical to the frozen roots. `SOURCE-MANIFEST.json` pins every header by path, byte count and SHA-256.

CMake installs the static target with `PUBLIC_HEADER`, exports `zg335Targets.cmake` under namespace `zg335::`, installs `zg335Config.cmake`/`zg335ConfigVersion.cmake` for `find_package(zg335 CONFIG)`, and exposes build/install include interfaces at `include/zg335`. The install tree is designed for host-level consumption; no install was executed in this task.

## 4. ABI and memory rules

- Require `CHAR_BIT == 8` and exact `uint32_t`, as already enforced by `zg116la_common.h`.
- Host-control structs are C ABI values for one process, not serialized wire layouts.
- Guest RAM, FS, provider userdata and callback tables remain live; only host tables/contexts and logical machine state are value-copied by the reviewed code.
- Output ownership stays with the caller. Configuration, provider control and guest backing must not alias the output.
- No thread-safety, concurrency, hostile callback, longjmp, fault or process-wide guarantee is added by the mobile port.
- Function pointers are never stored in guest words.

## 5. CMake layout

Use a normal CMake static library, not the fixture executable. The draft requires both `ZG_LEGACY_ROOT` and `ZG_ADAPTER_ROOT`:

```cmake
add_library(zg335_portable STATIC ${ZG335_SOURCES})
target_compile_features(zg335_portable PUBLIC c_std_11)
target_include_directories(zg335_portable PUBLIC include/zg335)
set_target_properties(zg335_portable PROPERTIES
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    PUBLIC_HEADER "${ZG335_PUBLIC_HEADERS}")
install(TARGETS zg335_portable EXPORT zg335Targets
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/zg335
    INCLUDES DESTINATION include/zg335)
install(EXPORT zg335Targets NAMESPACE zg335:: FILE zg335Targets.cmake
    DESTINATION lib/cmake/zg335)
```

Android:

- ABI targets: `arm64-v8a`, optionally `x86_64` for emulator.
- Use the NDK toolchain file from the Android host; do not vendor an NDK into the source tree.
- Prefer static library for engine embedding; shared library only if the host requires it.
- Export only the selected public symbols; hide all others with `-fvisibility=hidden` and explicit export macros added by the integration layer.

iOS:

- Build a static library for device and simulator slices, then combine with `lipo`/XCFramework at the host level.
- Deployment target and bitcode policy are host decisions; do not hardcode them in the portable source.
- No Objective-C or Apple framework dependency belongs in the C11 library.

## 6. Platform build preflight

| Check | Android | iOS |
|---|---|---|
| CMake >= 3.16 | host check | host check |
| C11 compiler | NDK clang | Apple clang |
| arm64 target | required | required |
| x86_64 | optional emulator only | simulator host-dependent |
| 8-bit byte / exact uint32 | compile-time `#error` | compile-time `#error` |
| fixture macros absent | required | required |
| Host runtime execution | not performed | not performed |

Current workstation state: no Android NDK or Xcode toolchain was invoked in this task. All platform build results are therefore UNVERIFIED, not passed.

Machine-readable result: `BUILD-PREFLIGHT.json`. At check time `cmake`, `ninja`, `clang`, `adb`, `ANDROID_NDK_HOME`, `ANDROID_NDK_ROOT` and `xcodebuild` were all unavailable. No compile, link, package or device run was attempted.

## 7. Integration sequence

1. Import the thirteen pinned C files and reviewed headers into a read-only vendor tree.
2. Add the portable CMake target and umbrella public header.
3. Build a host-side adapter that initializes `zg335116_config` and supplies the real memory/machine callbacks.
4. Run the existing F2/F3/F4 structural checks only as source regression evidence; do not treat them as mobile runtime proof.
5. Platform build and device smoke require separate authorization and a real Android/iOS toolchain.

## 8. Known inherited gaps

The five F4 runtime gaps remain excluded and do not become mobile acceptance by this design:

- ninth RETURNED complete dynamic R/W trace
- HOST_CAPTURE_INCOMPLETE actual execution
- three nonreturn runtime outcomes
- production overflow
- real F3 runtime

This design does not claim any of them.
