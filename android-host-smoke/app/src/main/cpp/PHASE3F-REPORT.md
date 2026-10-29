# Packed Asset Fallback Phase 3F Report

Task: T61. Implements `resources107.pak` fallback for the legacy UI, so
`three.module.js` and `GLTFLoader.js` resolve through the WebView asset path.
No protocol or record-layer changes.

## Implementation

Added `PackedAssetsPathHandler` and wired it to the `/assets/` path:

1. `AssetManager.open(path)` first; direct assets preserve their MIME.
2. On `FileNotFoundException`, lazily load/cache `resources107-index.json`
   (82,344 entries), key = path without leading `/`.
3. Open `resources107.pak` with `AssetFileDescriptor`, seek to
   `startOffset + indexOffset`, read only `length` bytes.
4. If index flag is `1`, wrap the bounded stream in `GZIPInputStream`.
5. Return `WebResourceResponse(mime, null, stream)`; the pak is streamed, never
   read wholly into memory.
6. Reject leading `/`, `..`, backslash and empty paths.
7. Negative index offsets (`offset=-1`, 80,457 entries) are treated as a safe
   miss; they are not read from offset 0 and do not produce GZIP errors.

`assets/resources107.pak` was previously deflated, which makes
`AssetFileDescriptor` unusable. `androidResources { noCompress += ["pak"] }` was
added; the APK now stores the pak with `compress_type=0` while the index remains
compressed (opened with `AssetManager.open`).

`/zg/render` remains a separate handler; no ordering conflict.

## Device evidence

MuMu `emulator-5554`:

```text
I ZG335ASSETS: index loaded entries=82344
I ZG335ASSETS: pak hit vendor/build/three.module.js offset=29766029 length=136590 gzip=1
I ZG335ASSETS: pak hit vendor/examples/jsm/loaders/GLTFLoader.js offset=29902619 length=28438 gzip=1
I ZG335ASSETS: pak hit vendor/examples/jsm/utils/BufferGeometryUtils.js offset=29931057 length=9122 gzip=1
I ZG335ASSETS: pak hit vendor/build/three.core.js offset=29440471 length=325558 gzip=1
I ZG335JS : ZG335JS asset vendor/build/three.module.js=200
I ZG335JS : ZG335JS asset vendor/examples/jsm/loaders/GLTFLoader.js=200
```

Negative entries are reported as misses, not errors:

```text
I ZG335ASSETS: pak miss negative audio107/footsteps.json offset=-1
I ZG335ASSETS: pak miss negative native107/particles107.json offset=-1
```

No `FileNotFoundException vendor/...` and no GZIP/read errors for the two key
vendor URLs. The legacy UI loaded; a full view51 scene is not required by this
task and was not used as a gate.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,817,624 | `22d850da58003a49818a603357b1bb91f0883bcad66dc30fd361fd441144a4b9` |
| `app-release-unsigned.apk` | 54,769,474 | `48158aa837b136957ef4be7e414e88640f39447f44f66756e00042b946c69855` |

Both APKs contain `assets/resources107.pak` with
`file_size=29,940,179`, `compress_size=29,940,179`, `compress_type=0`.

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `app/src/main/java/com/zg335/host/PackedAssetsPathHandler.java` | 8842 | `351a23a6da88464874868c3863bcd0c2b2861facfb577691b867acb5eae7bf5f` |
| `app/src/main/java/com/zg335/host/MainActivity.java` | 11901 | `e755a7d8ef7b1ea4e68fe3dcd941063bdbb76442f20713e88a6fba593ae05b8a` |
| `app/build.gradle` | 842 | `a7cb47ca86e1599ad1386c363aaae002170be6dd8c42308c2f3bf6acd660491c` |

No protocol/record-layer, old Java/assets, client.js or Three.js changes; no
push.
