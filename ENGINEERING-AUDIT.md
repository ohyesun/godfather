# Codemagic Bundle Engineering Audit

Status: **STATIC AUDIT PASS WITH WARNINGS; PLATFORM BUILD UNVERIFIED**
Audited: 2026-10-28
Scope: bundle-only static audit. No Host compilation, no signing, no device run,
no network push. Frozen design snapshot was read only.

## Verdict

The bundle is structurally usable as the input to Codemagic. YAML parses, the
frozen CMake design hash matches, all 13 production C files and all 14 packaged
headers are present and hash-identical to the frozen manifest after the bundle
repairs recorded below. One documentation FAIL remains: the README Git push
command contains a placeholder repository URL and therefore cannot be executed
literally. Platform build, SDK/OS/native-ABI and runtime acceptance remain
UNVERIFIED.

| Area | Result | Evidence |
|---|---|---|
| `codemagic.yaml` YAML syntax | PASS | PyYAML 6.0.3 `safe_load`; root mapping has `workflows` |
| Required workflow fields | PASS | Both workflows have `name/instance_type/max_build_duration/scripts/artifacts` |
| Android NDK/toolchain path | PASS (static) | `codemagic.yaml:10-41`; fallback checks `build/cmake/android.toolchain.cmake` |
| iOS CMake parameters | PASS (static) | `codemagic.yaml:59-67`; iOS/arm64/iphoneos/deployment target |
| Artifact paths | PASS (conditional) | install prefixes at `codemagic.yaml:39,65`; artifact patterns at `:47,73` |
| Design CMake against frozen b7eeb8fa | PASS | `design/CMakeLists.txt` SHA-256 `293c754e...` equals freeze pin for `CMakeLists.draft.txt` |
| Frozen design completeness | PASS after repair | `design/README.md`, `design/BUILD-PREFLIGHT.json`, `design/CMakeLists.draft.txt` restored |
| Vendor 13 C files | PASS | 13/13 SHA-256 match `SOURCE-MANIFEST.json` |
| Vendor 14 headers | PASS after repair | 13 reviewed headers + generated umbrella; 14/14 SHA-256 match |
| Design header closure | PASS | 14/14 pins match; all local includes resolve inside `design/include/zg335` |
| Fixture wrappers/hooks absent | PASS | No fixture source, fixture macro redirection or observed-trace generator in CMake/YAML |
| README command executability | **FAIL (documentation)** | `README.md:25` placeholder URL |
| Platform build/runtime | UNVERIFIED | No NDK/Xcode/Codemagic execution performed |

## Findings

### F-01 FAIL (documentation): README push command is not directly executable

`README.md:25` uses `<YOUR_GITHUB_OR_GITLAB_REPO_URL>` as the remote URL. Running
the block literally will fail at `git remote add`; `git push` is also outside
this audit and must not be run here.

Recommendation: replace the placeholder with the actual repository URL in the
operator's copy, or document `export REPO_URL=...` and use `"$REPO_URL"`. No
bundle source or frozen design change is needed.

### F-02 WARN: Android NDK selection is not fully hermetic

`codemagic.yaml:13-15` picks the newest NDK already present under `$SDK/ndk`;
the fallback later installs `27.0.12077973` (`:24`). If the image has multiple
NDKs, a newer/incompatible NDK may be selected before the fallback.

Recommendation: require the pinned version explicitly, for example select
`$SDK/ndk/27.0.12077973` when present and fail with an actionable message when
it is absent, or make the desired NDK version a workflow variable.

### F-03 WARN: optional CMake log artifact may not exist

`codemagic.yaml:49,75` list `build-*/CMakeFiles/CMakeOutput.log`. CMake may not
create `CMakeOutput.log` for every generator/configuration. This is normally an
optional artifact warning, not a build failure.

Recommendation: keep the artifact pattern only if a missing optional artifact is
acceptable, or remove it and retain `CMakeCache.txt`.

### F-04 WARN: image/tool versions are implicit

Neither workflow has an `environment` block. CMake, Xcode, Android SDK and
command-line tools therefore come from the selected `mac_mini_m2` image. The
YAML fields are valid Codemagic fields (`instance_type`, `max_build_duration`,
`scripts`, `artifacts`), but exact tool versions and image availability are not
proven by this static audit.

Recommendation: when platform execution is authorized, pin/record the Codemagic
image and tool versions in workflow metadata. Do not claim build acceptance
before that run.

### F-05 PASS with note: build directory and artifact relationship

The install prefixes use `$CM_BUILD_DIR/out/...` (`codemagic.yaml:39,65`) while
artifact patterns use `out/...` (`:47,73`). This is consistent when
`CM_BUILD_DIR` is the cloned repository root, which is Codemagic's documented
working directory semantics. No build was run to confirm the actual artifact
resolution on the selected image.

## Bundle repairs applied during audit

These changes are confined to the bundle; the frozen design snapshot was not
modified.

1. Restored `design/README.md` from the frozen design package.
   SHA-256 `152f046f0e42a63490e380a7948038bfb41b7d744b99288474086802ac93fa20`.
2. Restored `design/BUILD-PREFLIGHT.json` from the frozen design package.
   SHA-256 `16e32ee65692c4f3f5638a26df5461a616e33d893dcd6c08a5cf27ec6b066257`.
3. Added `design/CMakeLists.draft.txt` as a byte-identical compatibility copy
   of the build entry `design/CMakeLists.txt`; both SHA-256
   `293c754e9b33133a6c7ed4a431916797aae2c92c7978aa703cbc6fc67d77271c`.
4. Added the generated umbrella to the vendor payload at
   `vendor/generated/zg335_portable.h`; SHA-256
   `311eb3ca49375546474cd073dc251f1b7a4cba08085e1dbdef70c1df2ffbbe48`.
   This file is not a CMake build input; CMake consumes `design/include/zg335`.
5. Updated the bundle README to describe the restored design metadata and
   generated vendor umbrella, and to say the remote URL placeholder must be
   replaced before push.

## Hash inventory

| File | SHA-256 | Bytes |
|---|---|---|
| `codemagic.yaml` | `8ef42787e180ecd628969cd3c2fa4bb177dbb974f82eb72ca0e1c25a83cf59fa` | 3027 |
| `README.md` | `587b2dd2df78d6bcb71a26a23569d5a51c5c953af63aa61984e227922f5f5f7e` | 2096 |
| `design/CMakeLists.txt` | `293c754e9b33133a6c7ed4a431916797aae2c92c7978aa703cbc6fc67d77271c` | 4862 |
| `design/CMakeLists.draft.txt` | `293c754e9b33133a6c7ed4a431916797aae2c92c7978aa703cbc6fc67d77271c` | 4862 |
| `design/SOURCE-MANIFEST.json` | `b7eeb8fac58603f0b1da9da84627ae34ec09f853943c447c011dd8bface203d4` | 11973 |
| `design/BUILD-PREFLIGHT.json` | `16e32ee65692c4f3f5638a26df5461a616e33d893dcd6c08a5cf27ec6b066257` | 2630 |
| `design/README.md` | `152f046f0e42a63490e380a7948038bfb41b7d744b99288474086802ac93fa20` | 7473 |
| `design/DESIGN-FREEZE.json` | `dd9d432510aa349d9b6adfc98db1fc3405fa733a36543fae265b6593c3969368` | 1503 |
| `design/include/zg335/zg335_portable.h` | `311eb3ca49375546474cd073dc251f1b7a4cba08085e1dbdef70c1df2ffbbe48` | 597 |
| `vendor/generated/zg335_portable.h` | `311eb3ca49375546474cd073dc251f1b7a4cba08085e1dbdef70c1df2ffbbe48` | 597 |

## Verification commands used

- PyYAML `safe_load` on `codemagic.yaml`; workflow key/field inventory.
- SHA-256 and byte-size checks against `design/DESIGN-FREEZE.json` and
  `design/SOURCE-MANIFEST.json`.
- 13 source and 14 header hash checks against vendor payloads.
- Local include closure scan over `design/include/zg335/*.h`.
- Static source-path existence check for `${ZG_LEGACY_ROOT}` and
  `${ZG_ADAPTER_ROOT}` references in `design/CMakeLists.txt`.

## Not performed / not granted

- No `cmake` configure, compile, link, install, archive or XCFramework build.
- No Android emulator/device or iOS simulator/device execution.
- No Codemagic API/UI submission, Git remote push, signing or upload.
- No SDK, OS, native ABI, CRT, SEH, atomicity, allocation-ownership or runtime
  acceptance.
