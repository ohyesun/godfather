# QA Phase 3B2 — WebView Binary Endpoint

- Audit date: 2026-10-28
- Scope: `MainActivity.java`, embedded JS adapter, debug APK on MuMu, release APK scan
- Product source was read-only; no product/tests/Java/JS changes and no push.
- MainActivity SHA-256: `bf7312685ef77c4db3de336e33e0974ca905c49dfd11d611dbfdb0ea9e82a4f1` (10,366 bytes) — matches the task identity.

## 1. Verdict

**PASS for the WebView binary endpoint and release package hygiene.**

MuMu x86_64 independently served the 24-byte binary packet to the JavaScript adapter and the legacy UI loaded. The endpoint uses `application/octet-stream`, the no-packet path is HTTP 204, the JS path is fetch → arrayBuffer → Uint8Array, and no JSON/base64/hex/`evaluateJavascript` frame transport was added. The release APK hash matches and the p12/keystore scan is clean.

## 2. Source verification

`MainActivity.java:82-98`:

- `WebViewAssetLoader` registers `/assets/` and `/zg/`.
- `/zg/` PathHandler accepts all of `render`, `/render`, and `/zg/render`.
- A missing packet calls `noContent()`.
- A successful packet is returned as `application/octet-stream` with the 24-byte `ByteArrayInputStream`.

`MainActivity.java:189-193`:

```java
new WebResourceResponse("application/octet-stream", "UTF-8",
    204, "No Content", Collections.emptyMap(),
    new ByteArrayInputStream(new byte[0]));
```

`MainActivity.java:43-56` embedded JS:

```js
fetch('/zg/render',{cache:'no-store'})
  .then(function(response){
    if(response.status!==200)return null;
    return response.arrayBuffer();
  })
  .then(function(buffer){
    if(!buffer)return;
    var view=new Uint8Array(buffer);
    if(view.length!==24)return;
    var magic=(view[0]|(view[1]<<8)).toString(16).toUpperCase();
    var kind=view[3];
    console.log('ZG335JS render len='+view.length+' kind='+kind+' magic='+magic);
  })
  .catch(function(error){console.log('ZG335JS fetch-error '+error);});
```

There is no JSON, base64, hex, or `evaluateJavascript` frame path in MainActivity. The 204 path returns `null` before `arrayBuffer()`; it neither throws nor logs a fetch-error. Path mismatch also reaches `noContent()` and therefore returns 204, not 200. The WebMessage ArrayBuffer blocked limitation is not used as a pass basis; the source uses `fetch`/byte response instead.

## 3. Independent build and MuMu execution

Build: Gradle with JDK 17:

```text
BUILD SUCCESSFUL in 20s
```

Debug APK SHA-256: `8e10295ada051f70a2d52c144986653519b23ceecf5483cb61268207d9bce14a`.

Installed on `emulator-5554` (`com.zg335.host/.MainActivity`). Actual logcat capture:

```text
I/ZG335BRIDGE: webview=110.0.5481.154.1
I/ZG335BRIDGE: status=0 rx=24 frames=1 error=0
I/ZG335BRIDGE: frame status=0 rx=63 frames=2 error=0 before=24
I/ZG335JS: ZG335JS render len=24 kind=3 magic=5A47
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
I/ZG335JS: ZG335JS render len=24 kind=3 magic=5A47
...
```

The `ui loaded` line confirms the legacy `index.html` UI remains loaded. The repeated 24-byte `ZG335JS` lines are the actual WebView console bridge output, not a restatement of the author log.

## 4. Release APK

- Path: `app/build/outputs/apk/release/app-release-unsigned.apk`
- SHA-256: `961cd0d64b4a620ab87e4d5a456d1abc391ab902fac046ac329afdc950d91189` — matches.
- Zip entries: 1,820.
- Case-insensitive scan for `keystore|zg_test|.p12|.jks`: **0 matches**.
- `classes.dex` contains the T51 strings `/zg/`, `render`, `ZG335JS`, `fetch('/zg/render'`, `application/octet-stream`, and `No Content`; the release artifact is not an older pre-endpoint package.
- `WebMessage` appears in the dex as an AndroidX/library token, not as a frame transport in MainActivity; the blocked-WebMessage path is not claimed as acceptance.

## 5. Negative controls

- No packet: `pollRenderPacket()` returns null; server returns 204 with empty body and `application/octet-stream`; JS returns before parsing and no `ZG335JS fetch-error` is emitted.
- Non-matching path: PathHandler calls `noContent()` (204); it does not return HTTP 200.
- Invalid/oversized packet length: `pollRenderPacket()` returns null unless `pollRender` returns exactly 24; no partial binary frame is handed to JS.
- A 204 response is guarded by `response.status!==200`; it does not call `arrayBuffer()` or throw.

These are source-level checks; the device log confirms the normal path and UI load, but the no-packet 204 branch was not forced by a separate runtime control because the debug UI continuously polls a packet. This is a coverage note, not a source defect.

## 6. Findings summary

| Item | Result |
|---|---|
| `/zg/` PathHandler accepts relative `render` | PASS |
| 204 no-content response | PASS: `application/octet-stream`, empty body |
| 24-byte success response | PASS: octet-stream |
| JS fetch/arrayBuffer/Uint8Array | PASS |
| No JSON/base64/hex/evaluateJavascript frame path | PASS |
| Actual MuMu `ZG335JS` log | PASS: len=24 kind=3 magic=5A47 |
| Old UI still loads | PASS: `ui loaded .../assets/index.html` |
| Release APK hash | PASS: `961cd0d...` |
| Release p12/keystore scan | PASS: 0 matches |
| No-packet 204 / path mismatch negative controls | Source PASS; runtime 204 not separately forced |
| WebMessage blocked limitation | Not used as pass basis |

## 7. Boundary

This is local WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical-device, JNI/Java/JS whole-product, or game-command acceptance. No product source, tests, Java or JS was modified; no push was performed.
