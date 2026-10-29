# WebView ArrayBuffer Phase 3B2 Report

Task: T51. Scope: deliver the 24-byte CONTROL packet to WebView/JS over a binary
WebMessage ArrayBuffer, with no game fields and no client.js changes.

## Implemented

- Added `androidx.webkit:webkit:1.9.0`; `android.useAndroidX=true`.
- `WebViewAssetLoader` serves the legacy UI at:
  `https://appassets.androidplatform.net/assets/index.html`
- `shouldInterceptRequest` serves the original `index.html` bytes with a
  one-time inline adapter appended before `</body>`. Disk assets/client.js were
  not modified.
- `WebViewCompat.addWebMessageListener(web, "ZgBridgePort", ...)` is registered
  with origin `https://appassets.androidplatform.net`.
- Java calls `requestFull()` / `pollRender()`, then attempts
  `WebMessageCompat(byte[])` + `WebViewCompat.postWebMessage`.
- JS adapter reads `new Uint8Array(event.data)`, checks `ArrayBuffer`, reads
  magic/kind/header, and logs `ZG335JS ...`.
- `WebChromeClient` forwards `ZG335JS` console messages to logcat.
- No JSON/base64 or `evaluateJavascript` frame path was added.

## Device result: BLOCKED by WebView capability

Device: MuMu `emulator-5554`.

WebView version:

```text
I ZG335BRIDGE: webview=110.0.5481.154.1
```

Feature check:

```text
E ZG335BRIDGE: WebMessage ArrayBuffer unsupported
```

The Java-to-JS post fails:

```text
E ZG335BRIDGE: render post failed
E ZG335BRIDGE: java.lang.UnsupportedOperationException:
  This method is not supported by the current version of the framework and the
  current WebView APK
  at androidx.webkit.WebViewCompat.postWebMessage(WebViewCompat.java:524)
```

Therefore the required log

```text
ZG335JS render len=24 kind=3 magic=5A47
```

**was not produced**. The page adapter does execute, but no binary WebMessage
port is available:

```text
I ZG335JS : ZG335JS no-port
```

The legacy UI did load through the AssetLoader:

```text
I ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
```

C polling still succeeds independently:

```text
I ZG335BRIDGE: render len=24 kind=3 frame=1 magic=5A47
I ZG335BRIDGE: render again=2
```

## True ArrayBuffer?

**No.** `WebViewFeature.WEB_MESSAGE_ARRAY_BUFFER` is false on
`110.0.5481.154.1`; `postWebMessage(byte[])` throws
`UnsupportedOperationException`. Do not treat this run as ArrayBuffer PASS.

The target needs a WebView/Chrome provider that supports WebMessage ArrayBuffer,
or a different transport (e.g. native GL or a local binary channel) with its own
security review. That alternative was not implemented under this task's
no-fallback rule.

## APK

- Path: `app/build/outputs/apk/debug/app-debug.apk`
- Size: 44,446,344 bytes
- SHA-256: `e6b5f2ef85204e3ee8feaf5d8e00cc7df2f2c736e8cdbeaf7e595c1adb0a07e6`

## Release keystore exclusion after AndroidX

`./gradlew assembleRelease` succeeded after adding AndroidX WebKit.

- Debug APK contains exactly:
  `res/raw/zg_test_keystore.p12` (test-only, expected)
- Release APK: `app-release-unsigned.apk`
  - Size: 43,150,546 bytes
  - SHA-256: `fa1f90cc3bd0d87ed001923b8583c35c78e8fe433af3b4108ea20359ca133440`
  - Zip scan for `keystore|zg_test|.p12|.jks`: **0 hits**
- Release remains unsigned.
- AndroidX WebKit 1.9.0 dependency resolved successfully; no fallback was
  applied.

This re-establishes the T41 release-exclusion evidence under the new
AndroidX configuration. The WebMessage ArrayBuffer blocker above still stands.

## Files/hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `app/src/main/java/com/zg335/host/MainActivity.java` | 11786 | `77857ed9fc22535986e91ada2e0f33ec05336c20424356185515be5ada6f44bf` |
| `app/build.gradle` | 782 | `5daa535442674bf8d8a0e02476745a2a6196b3862967f5b213cb63e977577c13` |
| `gradle.properties` | 109 | `e6b436ff6a08da8b8c05e0e2fc1a7f40f41701bb506cd80fc2ddfd7b6e3b2b` |

Dependency: `androidx.webkit:webkit:1.9.0`.

No bridge ABI/parser, Java protocol bridge, old Java, client.js or assets were
modified; no push was performed.

## Resolution: WebViewAssetLoader binary endpoint PASS

The WebMessage ArrayBuffer blocker above is retained as history. The accepted
transport is the `WebViewAssetLoader` binary endpoint, which does not require
`WEB_MESSAGE_ARRAY_BUFFER`.

Root cause of the first endpoint attempt: AndroidX passes the path **relative to
the registered prefix without a leading slash** (`render` for `/zg/render`).
The handler originally accepted only `/render` and `/zg/render`, so every poll
returned 204. The handler now accepts `render`, `/render`, and `/zg/render`.

Verified on MuMu `emulator-5554`, WebView `110.0.5481.154.1`:

```text
I ZG335JS : ZG335JS render len=24 kind=3 magic=5A47
I ZG335JS : ZG335JS render len=24 kind=3 magic=5A47
...
```

The JS adapter fetches `/zg/render`, consumes `response.arrayBuffer()`, and
reads the 24-byte envelope as `Uint8Array`. No JSON/base64/`evaluateJavascript`
frame path is used.

Final debug APK:

- Size: 44,446,344 bytes
- SHA-256: `8e10295ada051f70a2d52c144986653519b23ceecf5483cb61268207d9bce14a`

Release recheck after the endpoint fix:

- `app-release-unsigned.apk`
- Size: check build output; SHA-256
  `961cd0d64b4a620ab87e4d5a456d1abc391ab902fac046ac329afdc950d91189`
- Zip scan `keystore|zg_test|.p12|.jks`: **0 hits** (`P12_ABSENT`)

Final source hashes:

| File | SHA-256 |
|---|---|
| `MainActivity.java` | `bf7312685ef77c4db3de336e33e0974ca905c49dfd11d611dbfdb0ea9e82a4f1` |
| `app/build.gradle` | `5daa535442674bf8d8a0e02476745a2a6196b3862967f5b213cb63e977577c13` |
| `gradle.properties` | `e6b436ff6a08da8b8c05e0e2fc1a7f40f41701bb506cd80fc2ddfd7b6e3b2b` |

Status: **PASS** for binary delivery of the 24-byte CONTROL packet to JS. This
does not accept game fields, Three.js scene updates, or full product runtime.
