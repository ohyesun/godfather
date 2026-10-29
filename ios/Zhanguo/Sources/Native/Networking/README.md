# iOS native networking contract

This directory implements networking only. The host owns WKWebView, local-main-document validation, a random document nonce, callback delivery, storage, app lifecycle and bundle configuration. No file in this directory evaluates JavaScript, accesses an account, logs payloads or retries an ambiguous write.

The authoritative Android r119 entry is `window.Lab.request95(id, endpoint, route, credential, account, bodyJSON)`, dispatched by `MainActivity$Bridge` to `TcpTransport96.request`. There is no `ZgNativeTCP` or `ZgNativeCodec` global in the delivered r119 client. Its `nativeReply` parses `reply.body` as JSON before resolving or rejecting. RC5, DES, continuous eight-byte game-block buffering, frame parsing, zlib and the game GBK encoder remain in the existing JavaScript modules.

## Host API

```swift
let profile = try JSONDecoder().decode(ZGConnectionProfile.self, from: bundledProfileJSON)
let networking = try ZGNativeNetworking(
    profile: profile,
    launcherAuthentication: privatelyDecodedBundledAuthenticationData
)
networking.replaceOwner(documentNonce)
networking.request(owner: documentNonce, endpoint: endpoint, route: route,
                   credential: credential, account: account, body: bodyJSON) { result in
    // The default callback queue is .main. Validate that this document nonce
    // is still current before delivering either a reply or an error.
    switch result {
    case .success(let reply): consume(reply.toDictionary())
    case .failure(let error as ZGNativeTransportError): consume(error.reply.dictionary)
    case .failure: consumeAControlledHostError()
    }
}
// On owner/document replacement, reload, app teardown, or explicit disconnect:
networking.cancelAll()
```

`ZGNativeTransportReply` exposes `status:Int`, `body:String`, `encoding:"identity"`, `wireBytes:Int`, `decodedBytes:Int`, plus `jsonObject`, `dictionary` and `toDictionary()` aliases. `ZGNativeTransportError` aliases `ZGNativeNetworkFailure`, whose public properties are `code`, `message`, and `delivery` (`.notSent` with raw value `not_sent`, or `.unknown`). Its `reply` is a controlled status503 envelope. Delivery `unknown` must never cause an automatic write retry.

The bundled config can decode its `profile` directly. Supported fields are `role`, `host`, `port`, `transport`, optional `cert`, `launcherHost`, `launcherPort`, and native-only `allowedGameEndpoints:[{host,port}]`. Direct mode is exactly `role=direct-game, transport=direct-tcp`; legacy pinned TLS mode is `role=server, transport=zg-tls1`. The UI control endpoint must exactly match the profile scheme/host/port. The current control/game profile port is6020; the launcher is independently configured at9999. No server address or authentication value is hardcoded here.

## Per-connection protocol for the iOS adapter

| Route | Input body | Success body |
|---|---|---|
| `/direct/open` | `{host,port}` | `{connected:true,connectionId:UUID,host,port}` |
| `/direct/send` | `{connectionId,packetHex}` | `{dataHex:"",connectionId}` |
| `/direct/receive` | `{connectionId}` | `{dataHex:hex,connectionId}` |
| `/direct/close` | `{connectionId}` | `{closed:true,connectionId}` |
| `/direct/zones` | `{}` | `{configText}` |
| `/direct/register` | `{account,password,qq,zone}` | `{ok,status,message}` |

Every `DirectJsonConnection` instance in the iOS-only compiled adapter must retain its own `connectionId` returned by open and attach it to its own send/receive/close calls. A global current connection ID is insufficient: a delayed close from an old instance would otherwise close its replacement. Missing or stale IDs return `stale_connection` without touching the current socket. A stale document owner similarly cannot send, poll, close or complete into a new owner.

Direct targets must be present in the bundled profile/native-only endpoint allowlist, or in a successful native launcher catalogue. Catalogue endpoints use the original `[Server]` fields3/4, optional game port7, and optional gateway9/10. Page-supplied IPv4 and port values alone do not authorize arbitrary TCP destinations. A native trusted allowlist may be needed for authenticated game redirects not already declared by a catalogue row. All these decisions are separate from the control endpoint validation.

## Wire and lifecycle details

- Raw game TCP keeps a bounded1MiB receive buffer, reads in at most64KiB chunks, and applies backpressure when full. Receive polls wait at most80ms for their first bytes and emit at most1MiB as hex. It never interprets a read as a game-frame boundary. Send does not steal receive bytes.
- Writes run through one serial queue with at most2MiB/64 queued requests. They recheck owner and socket identity immediately before native send. A failed or50-second timed-out native write is unknown and closes that connection; it is not replayed. Changing owner cancels connect, receive waits, queued writes, launcher transactions and the optional relay pipeline.
- Connect timeout is5seconds. Launcher connect and each protocol read/write stage are bounded by6seconds. Raw poll uses80ms. Optional legacy relay stages use50seconds, matching the Android role-entry budget.
- Launcher request header is12bytes little endian: version1, command, reservedu16, payload sizeu32, reservedu32. Reply is20bytes: version, status, reservedu16, payload sizeu32, file sizeu64, reservedu32, each body limited to2MiB. Commands9/2/8 retain the original version/file/registration exchange.
- The original Java file payload allocation is `20 + filename.length`, but it writes `16 + filename.length`. `LauncherConfig.txt` is18ASCIIbytes, so the actual payload is38bytes including four trailing zeros. These bytes are retained.
- Launcher text first uses strict UTF8 then the GBK subset through Foundation's GB18030 codec. Registration rejects characters outside the original GBK byte grammar instead of silently replacing them. Game text encoding remains in the original JS GBK table.
- Legacy TLS uses Network.framework TLS1.2+ with an exact bundled DER leaf pin, explicit DER validity dates and native trust evaluation against that pin. No permissive accept-any-certificate fallback exists. Relay frames remain18-byte big-endian `ZGTP` with matching request IDs,64KiB requests,4MiB responses. One TLS stream is reused and requests are serialized; failed requests are not retried.

## Validation boundary

`ios/ZhanguoTests/Networking/ZGNativeNetworkingTests.swift` contains offline fake-channel XCTest cases for golden protocol bytes, GBK/registration, endpoint and connection isolation, fragmented raw reads, owner cancellation, unknown writes, catalogue-derived destinations and persistent TLS envelope framing. No test connects to a real host.

`ios/ZhanguoTests/Networking/test_networking_contract.py` runs on the current Windows host. It independently checks original Java framing/secret-migration consistency without printing or recording the actual secret and validates the Swift interface/source contract. Its JSON explicitly marks `actualSwiftOrNetworkFrameworkExecution:false`. The Windows environment has no Swift compiler or Xcode. A Mac/iOS Simulator compile and the XCTest suite are still required; a Python pass is not an iOS runtime pass.

API references checked against primary Apple documentation: [NWConnection receive](https://developer.apple.com/documentation/network/nwconnection/receive(minimumincompletelength:maximumlength:completion:)), [NWParameters](https://developer.apple.com/documentation/network/nwparameters), [TLS client verification](https://developer.apple.com/documentation/network/creating-an-identity-for-local-network-tls), [GB18030 encoding](https://developer.apple.com/documentation/corefoundation/cfstringencodings/gb_18030_2000).
