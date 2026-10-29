# iOS JavaScript adaptation: offline acceptance

2026-10-16. The iOS overlay is generated from signed r119 APK `62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44`. Original Android assets and business TypeScript were not edited. `overlay-build.json` proves that client bytes preceding and following the replaced compiled transport class are unchanged.

The document-start bridge preserves the actual `Lab.request95` void/callback signature. It validates IDs and response bodies, ignores old owners and duplicate callbacks, clears pending work on page disposal, and never retries an uncertain action. Android synchronous offline APIs are deliberately absent; the separate storage API is asynchronous and checks strict boolean results. The host wire signatures are documented in `ios/Web/BRIDGE-CONTRACT.md`.

`test-contract.cjs` passed against the actual generated module factories and the original frozen client's `nativeReply` implementation. It executes original GBK encoding, compressed RC5 frames delivered in three-byte fragments, and a DES session-key roundtrip. The transport fixtures verify concurrent connections, late open/receive/decode, ID mismatch, explicit IDs on every send/read/close, and a held asynchronous decode followed by a second chunk. That second chunk cannot enter the same decoder concurrently or enter a later generation after reconnect. Native WK/TCP/storage replies in these fixtures are constructed test inputs.

`test-cold-start.cjs` passed in installed Microsoft Edge at 1280×720 and 390×844. It serves only manifest-verified overlay/original direct assets or requested indexed PAK spans. The actual index, startup module, game client and its ES modules initialize with zero uncaught page errors and zero external network requests. The login screen is visible; the aiming overlay remains hidden. Missing bridge produces the Chinese startup error before importing the game client.

The real launcher parser was also exercised with a constructed native JSON-string response containing seven zones. All seven appeared; selecting the seventh worked. No `/direct/open`, `/direct/send`, `/direct/receive`, login or registration was attempted. A gzip-compressed indexed PNG decoded to the original 24,153 bytes and an actual browser bitmap, with no erroneous `Content-Encoding` header. The final run served 193 resource requests without HTTP asset failures or manifest hash mismatches.

Screenshots are `cold-start-1280.png`, `cold-start-390.png`, `cold-start-zones-fixture.png`, and `cold-start-bridge-failure.png`. The JSON reports include the exact overlay manifest so a later rebuild cannot silently inherit an older result.

These are Windows Node/Edge and a localhost HTTP analogue of the native asset handler. They do not establish Swift compilation, Apple WKWebView custom-scheme ES-module behavior, Keychain durability, actual TCP/server login, background lifecycle correctness on a physical iPhone, frame rate, signing, or App Store readiness. The iOS runtime still checks GBK, secure random, actual zlib roundtrip and WebGL2. Safari/WebKit added Compression Streams in iOS 16.4 ([WebKit release notes](https://webkit.org/blog/13966/webkit-features-in-safari-16-4/)); minimum version and device acceptance remain host responsibilities.

Reproduce from the workspace root using the available Python/Node runtimes:

```text
python ios/Web/patch_client.py
node ios/tests/js/test-contract.cjs
node ios/tests/js/test-cold-start.cjs
```
