# Codemagic Bundle QA Audit

- Audit date: 2026-10-28
- Scope: `E:\zg-handoff-workbuddy\mobile-codemagic-bundle-20261028-v1`
- Mode: read-only static audit; no Host compilation, no git operation, no device run
- Current identity:
  - `codemagic.yaml` SHA-256 `b6111dbc5de67555bc49d2c1e9cc35eb8d89afc07272a9deccc96f6d5ee9b1e1` (6241 bytes)
  - `README.md` SHA-256 `d5a815d836955c406478ead63d05993364b1fd21510fac69f048525ceae2be24` (2436 bytes)
  - `design/DESIGN-FREEZE.json` SHA-256 `dd9d432510aa349d9b6adfc98db1fc3405fa733a36543fae265b6593c3969368` (1503 bytes)
- Overall result: **PASS (static readiness only)**
- Source/design/inventory result: **PASS**
- Platform build/runtime/signing result: **UNVERIFIED, not accepted**

## 1. Executive result

The current bundle is structurally ready to be submitted to Codemagic as a static C11 library build. Both workflows have executable CMake discovery and version checks, the NDK path is pinned to `27.0.12077973` and no longer uses `sort -V`, the README has a real `REPO_URL` guard instead of a literal placeholder, and the source/header inventory is closed.

This is not a platform build result. No Android NDK or Xcode compiler was invoked, no APK/AAR/IPA was produced, and no device, simulator, SDK, OS, ABI, signing or runtime acceptance is granted.

## 2. Inventory and frozen identity

- Production C translation units: **13**.
  - 8 under `vendor/legacy/`.
  - 5 under `vendor/adapter/`.
- Design public headers under `design/include/zg335/`: **14**.
  - 13 frozen reviewed header copies.
  - 1 generated umbrella `zg335_portable.h`.
- Vendor component headers: 13 build-necessary copies plus generated `vendor/generated/zg335_portable.h` (same `311eb3ca...` hash as the design umbrella). Total bundle headers: 28 (13 vendor component + 1 generated vendor umbrella + 14 design public). None is a fixture header.
- `design/DESIGN-FREEZE.json` pins all exist and match:
  - `SOURCE-MANIFEST.json` `b7eeb8fa...`
  - `CMakeLists.draft.txt` `293c754e...`
  - `README.md` `152f046f...`
  - `BUILD-PREFLIGHT.json` `16e32ee6...`
  - umbrella `311eb3ca...`
  - `cmake/zg335Config.cmake.in` `9338c78d...`
- `design/CMakeLists.txt` and `design/CMakeLists.draft.txt` are byte-identical (`293c754e...`).
- All 13 vendor C files match the source manifest and frozen bytes/SHA.
- All 14 design public headers match their pins and local include closure is complete.
- No fixture source, fixture checker, test oracle, controlled-overflow wrapper, `fixture_*` macro redirect, `BODY`, or `rfind` is present in production source.

## 3. Codemagic YAML and shell structure

`codemagic.yaml` parses as a dictionary with exactly these workflows:

- `android-portable`
- `ios-portable`

Each workflow has `name`, `instance_type: mac_mini_m2`, a numeric `max_build_duration`, `scripts`, and `artifacts`. All 7 embedded scripts pass `bash -n` syntax checking.

### Android workflow

- `codemagic.yaml:7-29` locates NDK 27.0.12077973 and, if absent, installs that exact version with `sdkmanager`. It no longer enumerates arbitrary NDK versions and contains no `sort -V`.
- `codemagic.yaml:30-65` resolves CMake:
  - prefers `$SDK/cmake/3.22.1/bin/cmake`;
  - otherwise enumerates executable CMake candidates under `$SDK/cmake/*` and selects the highest version with numeric sort;
  - falls back to PATH and Homebrew;
  - checks `cmake >= 3.16` and persists `CMAKE_BIN`.
- `codemagic.yaml:66-83` configures and builds `arm64-v8a` with `ANDROID_PLATFORM=android-24`, vendor roots and the `install` target.
- `codemagic.yaml:84-86` requests the installed tree and CMake cache.

### iOS workflow

- `codemagic.yaml:93-120` resolves CMake from PATH, Homebrew paths, or Homebrew installation, checks `>=3.16`, and persists `CMAKE_BIN`.
- `codemagic.yaml:121-138` uses `CMAKE_SYSTEM_NAME=iOS`, `CMAKE_OSX_ARCHITECTURES=arm64`, `CMAKE_OSX_SYSROOT=iphoneos`, deployment target 13.0, and builds/installs the static library.
- `codemagic.yaml:139-141` requests the installed tree and CMake cache.

## 4. Artifacts

`CMAKE_INSTALL_PREFIX` points to `$CM_BUILD_DIR/out/<target>` and artifacts use `out/<target>/**`:

- `codemagic.yaml:76,84-86` Android.
- `codemagic.yaml:131,139-141` iOS.

Given Codemagic's `CM_BUILD_DIR` root semantics, these paths are structurally valid. `design/CMakeLists.txt:82-103` installs the archive, public headers, target export, and Config/ConfigVersion files. The workflows no longer request the optional `CMakeOutput.log`, removing that stale-artifact risk.

## 5. README and claims review

Current `README.md:19-33` uses `REPO_URL` with an empty-value guard; it no longer contains a placeholder URL that fails literally.

The README does not claim runtime, signing, APK/AAR/IPA, device, SDK, OS or ABI acceptance. It explicitly states:

- no fixture wrapper, macro redirection, Host runner, signing identity or runtime oracle;
- output is a static library plus installed headers, not an app package;
- simulator slices, APK/AAB/IPA packaging/signing, and device tests are not covered.

The claim boundary is accurate.

## 6. Remaining warnings (not static blockers)

- The resolved NDK is pinned to 27, but if that exact package is absent the workflow depends on `sdkmanager` network/download availability. Record the installed NDK version in build evidence.
- `design/CMakeLists.txt:78` uses `-Werror`; no Android/iOS compiler run exists yet. A benign compiler warning can become a first-run failure.
- The selected Codemagic image/toolchain versions are implicit; record them with the first build.
- `design/SOURCE-MANIFEST.json` records external provenance roots while Codemagic builds from `vendor/...`; vendor hashes match, but a bundle-local vendor map would make CI-side verification more reproducible.
- Actual compile/link/install, SDK/OS/ABI, signing, device/simulator and runtime remain unverified.

## 7. Verdict

**PASS (static readiness only)** for submission to Codemagic. The earlier CMake selection defect is resolved by the current `b6111dbc...` workflow. Do not report this as a successful Android/iOS build until an authorized Codemagic run completes and its artifacts and toolchain versions are recorded.
