# iOS Web bridge contract, version 1

The source is the frozen signed r119 APK (`62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44`). Android assets and business TypeScript are not changed. `patch_client.py` creates a separate, manifest constrained `Resources/GameOverlay`; `GameAssets/files` stays byte identical.

## Host injection and origin

The WK host reads overlay `native-ios.js`, replaces its single bare token `__ZG_IOS_BOOTSTRAP_JSON__` with a serialized JSON **object literal**, then injects that source at document start, main frame only:

```js
const bootstrap = {"v":1,"nonce":"host-generated-private-page-owner"};
```

The source is not an HTML script. The nonce is held only in its closure. The host accepts messages only from its current main frame at `zgasset://bundle` with version 1 and the current nonce. All assets retain relative URLs at `zgasset://bundle/index.html`. Asset fetches, ES modules, images, WebGL buffers and audio use the asset scheme handler, not the JSON bridge. The overlay index imports `ios-startup.js`; that module checks the bridge, secure randomness, actual GBK decoding, zlib stream roundtrip and WebGL2 before importing the game entry.

## Messages and callbacks

Channel: `window.webkit.messageHandlers.zgNative.postMessage(message)`. Use ordinary `WKScriptMessageHandler`, not `WithReply`.

```js
{v:1,kind:"request95",nonce,id,endpoint,route,credential,account,body}
```

`Lab.request95(id, endpoint, route, credential, account, body)` returns **void**. `id` is a positive safe integer. All other arguments are strings; `body` is an encoded JSON object. Allowed routes are `/direct/open`, `/direct/send`, `/direct/receive`, `/direct/close`, `/direct/zones`, `/direct/register`. The host validates profile addresses independently; strings from JavaScript do not confer address authority.

The host evaluates a safely JSON serialized call to `ZgNativeIOS.receive(envelope)`:

```js
{v:1,kind:"request95",nonce,id,ok:true,
 result:{status:200,body:"{\"connected\":true,\"connectionId\":\"UUID\"}",encoding:"identity"}}
{v:1,kind:"request95",nonce,id,ok:false,
 error:{code:"native_failure",message:"…",delivery:"unknown"}}
```

`result.body` must remain a **JSON string**; the original game's `nativeReply(id,result)` parses it and resolves status 200 or rejects other status codes. Unknown IDs, duplicate replies and old nonces are ignored. Responses do not retry actions. Pending native requests have a 55 second bridge timeout (the existing game may time out sooner); timeout/dismissal is delivery `unknown`, not proof of failure to send. Explicit local validation/post rejection is `not_sent`. Report messages `{v:1,kind:"report",nonce,body:jsonString}` are diagnostic only, with no action result.

## Per connection identity

The iOS overlay replaces only the compiled `DirectJsonConnection` class. RC5, DES, framing, zlib and GBK implementations remain the exact original source bytes surrounding that class.

* `open` body `{host,port}` returns `{connected:true,connectionId:UUID,host,port}`.
* `send` body `{connectionId,packetHex}` returns `{dataHex:"",connectionId}`.
* `receive` body `{connectionId}` returns `{dataHex:hexString,connectionId}`.
* `close` body `{connectionId}` returns `{closed:true,connectionId}`.

Each JavaScript connection instance captures its own ID and generation. Close after a late open closes only the returned old ID. A late receive/send/decode cannot change a later connection or publish its bytes. Swift rejects an unowned or stale ID with status 503 and JSON body `{error,code:"stale_connection",delivery:"not_sent"}`, without closing the current socket. No global current ID exists in the shim.

## Storage: asynchronous, explicitly separate

The Android legacy `offlineDevice49()` / `offlineRead49()` return strings synchronously, while `offlineRecord49()` / `offlineForget49()` return a strict persisted boolean synchronously. A WK message cannot implement those synchronous signatures. They are **not defined** as fake successes or Promise substitutions. The current direct mode does not support offline hosting.

New APIs are `ZgNativeIOS.storage.device()`, `.read()`, `.record(jsonString)`, `.forget()`, all returning Promises. The messages are `{v:1,kind:"storage",nonce,id:"storage:N",operation,body?}`. Reply uses the same `kind`, ID and nonce: `ok:true,result:<JSON string>` for device/read (read may be empty string), and `result:<strict boolean>` for record/forget. True must mean a real durable Keychain write/delete according to the host contract. Error uses the same `ok:false,error` shape. The host filters persisted fields. The Web shim does not store game passwords/tokens.

Legacy `Lab.request`, relay stream methods, native codec and `readPacked107` are not fabricated. Static audit of all direct ZIP JavaScript/HTML found no `readPacked107` call. The four additional visible packed JavaScript files (Three core/module, GLTFLoader, BufferGeometryUtils) were read by their verified indexed spans and contain no native bridge markers. Scheme assets replace Android asset serving; this audit does not decode unrelated binary game assets.

## Runtime and validation limits

Minimum target is iOS 16.4, where Safari/WebKit added Compression Streams. Runtime checks remain necessary. A missing `crypto.randomUUID` is supplied only by real `getRandomValues`; there is no weak random fallback. Missing bridge/GBK/zlib/WebGL2 yields a visible Chinese startup error and no game import.

Windows Node/Edge fixtures use actual generated assets and real JavaScript codecs, with a mocked WK message handler and native responses. They validate JavaScript contracts and browser startup, not Swift compilation, iOS WKWebView behavior, Keychain durability, real TCP/game login, device performance or App Store signing. Native XCTest and physical iOS acceptance are separate required work.
