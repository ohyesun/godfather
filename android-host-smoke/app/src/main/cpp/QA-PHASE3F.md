# QA Phase 3F — Packed Asset Fallback

- Audit date: 2026-10-28
- Scope: `PackedAssetsPathHandler.java`, `MainActivity.java`, `app/build.gradle`, `resources107-index.json`, `resources107.pak`, debug/release APKs
- Mode: read-only product source; independent Gradle debug build; MuMu `emulator-5554` execution; no product/tests/Java/JS changes; no push.
- Frozen identities verified:
  - `PackedAssetsPathHandler`: `351a23a6da88464874868c3863bcd0c2b2861facfb577691b867acb5eae7bf5f` (8,842 bytes)
  - `MainActivity`: `e755a7d8ef7b1ea4e68fe3dcd941063bdbb76442f20713e88a6fba593ae05b8a` (11,901 bytes)
  - `app/build.gradle`: `a7cb47ca86e1599ad1386c363aaae002170be6dd8c42308c2f3bf6acd660491c` (842 bytes)

## 1. Verdict

**PASS for packed asset fallback, vendor loading, pak storage and release hygiene.**

Both vendor URLs returned HTTP 200 on MuMu via the packed fallback. The index offsets and lengths match the declared values, gzip entries decompress to valid module text, negative offsets miss, `/zg/render` remains unaffected, and both debug/release pak entries are stored (`compress_type=0`). No p12/keystore material is in the release APK.

## 2. Index and pak payload verification

Index entries:

```text
vendor/build/three.module.js                  [29766029, 136590, 1]
vendor/examples/jsm/loaders/GLTFLoader.js     [29902619, 28438, 1]
```

Independent decompression from `resources107.pak`:

| Asset | offset | compressed length | gzip | plain length | content |
|---|---:|---:|---:|---:|---|
| `vendor/build/three.module.js` | 29766029 | 136590 | 1 | 603113 | starts with Three.js license/module text |
| `vendor/examples/jsm/loaders/GLTFLoader.js` | 29902619 | 28438 | 1 | 114739 | starts with `import { AnimationClip, Bone, Box3...` |

Both are valid gzip members and produce readable ES-module JavaScript. `resources107-index.json` loaded with **82,344** entries; 80,457 entries have a negative offset and are correctly treated as fallback misses unless a direct asset exists.

## 3. Handler logic

`PackedAssetsPathHandler.java`:

- `handle()` first calls `assets.open(key)` (lines 53-60); only `FileNotFoundException` falls through to the pak. A non-FileNotFound IOException is logged and still reaches the fallback path.
- Negative offset or length returns `null` / `pak miss negative` (lines 73-76). No gzip decode is attempted in that case.
- `normalize()` (lines 98-108) strips one leading slash, rejects empty, leading slash, any `..`, and backslash. Rejected paths return `null` (no 200 response).
- Pak reading uses `AssetFileDescriptor` + `FileInputStream` + `FileChannel.position(start)` + `BoundedInputStream` (lines 78-84). It does not read the whole 29.9 MB pak into memory; only the index JSON is read into memory.
- `gzip == 1` wraps the bounded stream in `GZIPInputStream` (lines 84-88).
- The `/zg/` handler remains separate in MainActivity and still returns the 288-byte render stream; `/assets/` is the packed fallback handler.

## 4. APK pak storage

| APK | SHA-256 | pak entry | compress_type | size |
|---|---|---|---:|---:|
| debug | `22d850da58003a49818a603357b1bb91f0883bcad66dc30fd361fd441144a4b9` | `assets/resources107.pak` | 0 (stored) | 29,940,179 |
| release | `48158aa837b136957ef4be7e414e88640f39447f44f66756e00042b946c69855` | `assets/resources107.pak` | 0 (stored) | 29,940,179 |

The pak's compressed size equals its file size in both APKs, confirming stored/seekable packaging. `build.gradle` uses `noCompress += ["pak"]`.

## 5. MuMu execution

Installed the T61 debug APK on `emulator-5554`. Actual logcat:

```text
I/ZG335ASSETS: index loaded entries=82344
I/ZG335ASSETS: pak hit vendor/build/three.module.js offset=29766029 length=136590 gzip=1
I/ZG335ASSETS: pak hit vendor/examples/jsm/loaders/GLTFLoader.js offset=29902619 length=28438 gzip=1
I/ZG335JS: ZG335JS asset vendor/examples/jsm/loaders/GLTFLoader.js=200
I/ZG335JS: ZG335JS asset vendor/build/three.module.js=200
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
```

The full log scan contains no `FileNotFoundException vendor` and no `ZG335JS asset-error`. Negative-offset entries log:

```text
I/ZG335ASSETS: pak miss negative audio107/actions.json offset=-1
I/ZG335ASSETS: pak miss negative native107/particles107.json offset=-1
```

The 288-byte render path remains active in the same session (`ZG335JS full=288 records=7...`).

## 6. Release hygiene

Release APK:

- SHA-256: `48158aa837b136957ef4be7e414e88640f39447f44f66756e00042b946c69855`
- `resources107.pak` stored (`compress_type=0`)
- Case-insensitive zip scan `keystore|zg_test|.p12|.jks`: **0 matches**

## 7. Findings summary

| Item | Result |
|---|---|
| Index vendor offsets/lengths/gzip | PASS |
| gzip vendor modules readable | PASS |
| Direct asset first / FileNotFound fallback | PASS |
| negative offset miss, no gzip read | PASS |
| literal `..`/backslash path rejection | PASS |
| no whole-pak read | PASS: descriptor + bounded stream |
| `/zg/render` unaffected | PASS: 288 path still logs |
| debug pak stored | PASS: type0 |
| release pak stored | PASS: type0 |
| vendor 200 / no FileNotFoundException | PASS |
| release p12/keystore | PASS: 0 |

Coverage note: percent-encoded traversal (`%2e%2e`) was not separately forced on the device; the literal `..` path rejection is present in source. The index itself is loaded into memory (3.38 MB), but the 29.9 MB pak is not.

## 8. Boundary

This is local Java/WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, whole-game asset correctness, or game-field semantics. No product source, tests, Java or JS was modified; no push was performed.
