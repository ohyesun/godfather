# iOS local assets

The source is the signed r119 APK, SHA256
`62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44`.
Run `python3 ios/tools/prepare_assets.py` from the bridge workspace. The default
destination is `ios/Zhanguo/Resources/GameAssets`. A repeated run verifies the
existing files; it never overwrites edits, deletes an existing output, or repairs
an unrelated directory. `--verify-only` also checks every direct file and the
original PAK hash. Source APK bytes remain unchanged.

Actual preparation on Windows completed for all 1,781 direct assets and all
82,344 PAK members. The compact manifest records each decoded payload's SHA256,
length, original slice offset/length and codec. There are 64,353 gzip members and
17,991 identity members in this exact package. The decoder also supports zlib
members, covered by a constructed preparation fixture. Decoded packed bytes
total 2,255,979,051; the largest model is 24,449,744 bytes. The 1,641,386,457-byte
PAK stays unchanged on disk. No startup extraction into 82,344 files is needed.

Use one resource origin: `zgasset://bundle/index.html`. Register
`ZGAssetSchemeHandler(rootURL:overlayURL:)` with `WKWebViewConfiguration` before
constructing the web view. `GameAssets` contains `files/` and
`assets-manifest.json`. Optional `GameOverlay` contains platform-specific
`index.html`, `client.js`, and native/startup shims plus `overlay-manifest.json`.
The overlay format is:

```json
{"schemaVersion":1,"sourceAPK_SHA256":"62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44","files":{"native-ios.js":[123,"64-hex-sha256"]}}
```

Resolution order is overlay, direct APK asset, indexed PAK member. The 1,722
direct assets that overlap old packed copies must win; otherwise r119's patched
client and view would be replaced by older code. Relative `fetch`, ES modules,
GLB/TextureLoader, scene chunks, movie images and audio retain their paths.
Query strings do not change resource identity. Neither absolute file access nor
a cross-host proxy is exposed. `NativeConfig.json` and manifests are outside the
published resource maps and cannot be fetched through this handler.

The native implementation supports GET, HEAD, OPTIONS and a single byte Range
over the decoded representation. It returns 206/416 and Content-Range, handles
suffix/open-ended ranges, ignores Range for HEAD, and rejects multipart ranges.
MIME covers the actual JS/JSON/CSS/HTML/WebP/PNG/GLB/MP3/WAV/OGG/bin resources and
`application/wasm`. The r119 package contains no `.wasm` payload. Decoded bodies
have no Content-Encoding, preventing WebKit from decompressing them twice.
Local responses use no-store to avoid carrying stale r119 code across app
updates. CORS headers permit local module/image consumers without enabling
credentialed external requests.

Link `libz.tbd`; `ZGAssetReader` imports the Apple SDK's `zlib` module. Each load
owns a FileHandle and inflater. Three worker operations stream 64KB chunks and
wait for main-actor delivery acknowledgement, so a large resource cannot enqueue
unbounded native Data. A compressed range is inflated sequentially only as far
as needed. Full decoded loads verify the member's zlib/gzip end and CRC; SHA256
provenance is validated during preparation/build verification rather than
rehashed on every texture request. The signed app bundle is immutable at runtime.

All WK task callbacks and stop requests are serialized on the main actor.
Queued callbacks recheck both task and cancellation identities. Stop invokes no
didReceive/didFinish/didFail callback, consistent with Apple's task lifecycle.
`cancelAll()` retires a prior WK view while allowing the handler/store to be
reused by the replacement view. No game socket, account or zone state is touched.

Windows results are in `ios/tests/asset-preparation-audit.json`; six Python test
methods pass, including source SHA mismatch, all codecs, idempotence, external
edits, path/symlink/offset bounds, and compressed CRC/trailing-data rejection.
Independent gzip decoding of the real rabbit model, largest model, Three module
and original texture also matched the prepared hashes and GLB lengths.

`ZhanguoTests/Assets` provides Apple-only XCTest coverage for the actual Swift
store/reader, ranges/HEAD/overlay/path bounds and cancellation after response,
plus an actual WKWebView smoke test for import maps/ESM/fetch/Range/HEAD/PNG
texture upload to WebGL2/audio decoding/streaming WebAssembly MIME. These Swift
tests have **not** been compiled or run on Windows. Build and run on a Mac with
the Apple SDK, then repeat the full game's model, scene and audio acceptance on
the target iPhone. Import maps require Safari/iOS 16.4 or newer; the app's chosen
deployment target must account for that.

API references:

- [WKURLSchemeHandler](https://developer.apple.com/documentation/webkit/wkurlschemehandler)
- [WKURLSchemeTask incremental delivery and stop rules](https://developer.apple.com/documentation/webkit/wkurlschemetask)
- [Safari 16.4 import-map support](https://developer.apple.com/documentation/safari-release-notes/safari-16_4-release-notes)
