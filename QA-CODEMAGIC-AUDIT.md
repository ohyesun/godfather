# Codemagic Bundle QA Audit

- Audit date: 2026-10-28
- Scope: `E:\zg-handoff-workbuddy\mobile-codemagic-bundle-20261028-v1`
- Mode: read-only static audit; no Host compilation, no git operation, no device run
- Current bundle files audited:
  - `codemagic.yaml` SHA-256 `5fdf4047268fdf31da21e69416b80035041a12ba065d4b1937fffb9c7ee03a39` (6292 bytes)
  - `README.md` SHA-256 `d5a815d836955c406478ead63d05993364b1fd21510fac69f048525ceae2be24` (2436 bytes)
  - `design/DESIGN-FREEZE.json` SHA-256 `dd9d432510aa349d9b6adfc98db1fc3405fa733a36543fae265b6593c3969368` (1503 bytes)
- Overall result: **FAIL for first cloud build readiness**
- Source/design/inventory result: **PASS**
- Runtime/signing result: **not claimed, correctly excluded**

## 1. Executive result

The bundle contains the intended production C set, public header set, frozen design and build-necessary vendor payload. The YAML and shell syntax are valid at the static level. The previous NDK `sort -V` blocker and the README placeholder-URL documentation blocker are repaired in the current files.

One first-build blocker remains in the current CMake discovery logic: when no standalone `cmake` is on `PATH`, Android selects the first CMake binary found anywhere under `$ANDROID_SDK_ROOT/cmake` (`codemagic.yaml:42-44`) and then rejects the workflow if that first binary is older than 3.16 (`codemagic.yaml:58-60`). It does not select the highest available CMake or fall back to another discovered binary. Codemagic's documented Android SDK contains multiple CMake packages, including older 3.6/3.10 versions, so the first match can deterministically fail the version gate.

This report therefore marks the bundle FAIL for first cloud build readiness until CMake selection is made deterministic and version-safe. No runtime, SDK, OS, ABI, signing, APK, AAR, IPA or device claim is granted.

## 2. Inventory and frozen identity

Verified:

- Production C translation units: **13**.
  - 8 under `vendor/legacy/`.
  - 5 under `vendor/adapter/`.
- Design public headers under `design/include/zg335/`: **14**.
  - 13 frozen reviewed header copies.
  - 1 generated umbrella `zg335_portable.h`.
- Vendor component headers: 13 build-necessary copies (8 legacy + 5 adapter), plus the generated vendor umbrella. They are not fixture files.
- `design/DESIGN-FREEZE.json` pins all exist and match their actual files:
  - `SOURCE-MANIFEST.json` `b7eeb8fa...`
  - `CMakeLists.draft.txt` `293c754e...`
  - `README.md` `152f046f...`
  - `BUILD-PREFLIGHT.json` `16e32ee6...`
  - `include/zg335/zg335_portable.h` `311eb3ca...`
  - `cmake/zg335Config.cmake.in` `9338c78d...`
- `design/CMakeLists.txt` and `design/CMakeLists.draft.txt` are byte-identical (`293c754e...`).
- All 13 vendor C files match the source manifest and frozen source bytes/SHA.
- All 14 design public headers have matching paths, bytes and SHA pins; local `#include "..."` closure is complete.
- No fixture source, fixture checker, test oracle, controlled-overflow wrapper, `fixture_*` macro redirect, `BODY`, or `rfind` is present in the production source set. README/manifest occurrences are explanatory/exclusion text, not compiled code.

## 3. Codemagic YAML and shell structure

`codemagic.yaml` parses as a dictionary with exactly these workflows:

- `android-portable`
- `ios-portable`

Each workflow has `name`, `instance_type: mac_mini_m2`, a numeric `max_build_duration`, `scripts`, and `artifacts`. All embedded scripts pass `bash -n` syntax checking. The YAML structure is valid for Codemagic.

### Android workflow

- `codemagic.yaml:7-36` locates an NDK. The current code prefers exact `27.0.12077973`, then uses portable numeric sorting (`sort -t. -k1,1n -k2,2n -k3,3n`) and finally falls back to `sdkmanager`. The former `sort -V` blocker is resolved.
- `codemagic.yaml:37-62` resolves CMake from PATH or the Android SDK, with a Homebrew fallback and a `>=3.16` check.
- `codemagic.yaml:63-80` configures and builds the Android `arm64-v8a` static library with `ANDROID_PLATFORM=android-24`, vendor roots and `install`.
- `codemagic.yaml:81-84` requests the installed tree and diagnostics.

### iOS workflow

- `codemagic.yaml:91-118` resolves CMake from PATH, explicit Homebrew paths, or Homebrew installation, checks `>=3.16`, and persists `CMAKE_BIN`.
- `codemagic.yaml:119-136` uses `CMAKE_SYSTEM_NAME=iOS`, `CMAKE_OSX_ARCHITECTURES=arm64`, `CMAKE_OSX_SYSROOT=iphoneos`, deployment target 13.0, and builds/installs the static library.
- Simulator slices, signing and packaging are excluded in the README and are not claimed here.

## 4. Artifacts

`CMAKE_INSTALL_PREFIX` points to `$CM_BUILD_DIR/out/<target>` and artifact paths use `out/<target>/**`:

- `codemagic.yaml:73,81-84` Android.
- `codemagic.yaml:129,137-140` iOS.

Given Codemagic's documented `CM_BUILD_DIR` clone-root semantics, these paths are structurally valid. The install rules in `design/CMakeLists.txt:82-103` install the archive, public headers, target export, and Config/ConfigVersion files. No artifact-path blocker found. `build-*/CMakeFiles/CMakeOutput.log` may be absent on some generators; this is an artifact warning, not a build blocker.

## 5. First-cloud-build blockers

### B1 — CMake SDK discovery picks the first, not the newest, binary (BLOCKER)

Evidence:

- `codemagic.yaml:42-44`:
  `CMAKE_BIN="$(find "$SDK/cmake" -type f -path '*/bin/cmake' | head -1)"`
- `codemagic.yaml:58-60` rejects any CMake older than 3.16.
- Codemagic's documented Android SDK lists CMake packages such as `3.6.4111459`, `3.10.2.4988404`, `3.18.1`, `3.22.1`, and `4.0.2`.
- If `command -v cmake` is empty and `find` returns a 3.6/3.10 binary first, the script exits before configure even though a newer CMake package is present. It does not try the next match or fall back to Homebrew once a non-empty older binary is selected.

Required fix: discover all `*/bin/cmake` candidates and select the highest version, or explicitly prefer a known CMake >=3.16 package; only then run the version gate. Add a fallback that runs when the first discovered binary is too old.

### B2 — Old NDK fallback blocker is resolved, but NDK version is not pinned on the image (WARN)

The previous `sort -V` defect is fixed at `codemagic.yaml:16-18`. If NDK 27 is absent, the fallback intentionally chooses the highest installed NDK (currently potentially 29.x on the image). This is not a first-build blocker, but it means the actual NDK version is image-dependent. Pin or record the resolved NDK in build metadata.

### B3 — README push placeholder is resolved (PASS)

Current `README.md:19-32` exports `REPO_URL` and fails early when it is empty; it no longer contains an executable placeholder URL. The old documentation FAIL from the prior engineering audit is not present in the current file.

## 6. README and claims review

The README does **not** claim runtime, signing, APK/AAR/IPA, device, SDK, OS or ABI acceptance. It explicitly says:

- no fixture wrapper, fixture macro redirection, Host runner, signing identity or runtime oracle;
- the output is a static library plus headers, not an app package;
- simulator slices, APK/AAB/IPA packaging/signing, and device tests are not covered.

That claim boundary is accurate. The “self-contained repository payload” description is true for the compiled vendor tree and design headers; the design source manifest still contains external provenance roots, so CI-side hash validation is not bundle-relative. That is a provenance/documentation note, not a current build blocker.

## 7. Secondary warnings

- `design/CMakeLists.txt:78` enables `-Werror`. No Android/iOS compiler has been run; a benign platform warning can become a first-build failure. Add compiler-specific preflight or relax this for the first platform build.
- `codemagic.yaml` has no explicit `environment` block pinning Xcode, Android SDK, NDK or CMake image versions. Record the selected image/tool versions with the build evidence.
- `build-*/CMakeFiles/CMakeOutput.log` is not guaranteed to exist on every CMake generator/configuration; keep it as optional or remove it from required artifacts.
- `design/SOURCE-MANIFEST.json` uses external `D:/...` and `E:/...` roots, while Codemagic builds from `vendor/...`. Vendor hashes match in this audit, but a bundle-local vendor map would make CI validation reproducible.

## 8. Verdict

**FAIL for first cloud build readiness** until B1 is fixed. The static source/header closure, frozen design pins, YAML syntax, shell syntax, artifact structure and README claim boundary pass.

After fixing CMake discovery to select a version >=3.16 deterministically, rerun this audit on the resulting bundle. Platform build, simulator/device execution and signing remain unverified and unaccepted.
