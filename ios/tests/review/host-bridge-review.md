# Independent iOS Host, bridge and Keychain source review

The requested App, Native/Bridge and Native/Storage sources were read without changing them. The implementation owner fixed the actionable issues below; independent test/report files were added separately. This Windows review is not a Swift compilation or an Apple WKWebView/Keychain execution claim.

## Findings and fixes

1. **Storage record never reached Keychain.** The actual `native-ios.js` message uses `{kind:'storage', operation:'record', body:value}`. The original Host read `body["value"]`, always passing nil to `ZGSecureStorage.record`. The Host now reads the actual `body` field. The report message had the same `body` versus `text` mismatch and is also corrected.
2. **Same-index document replacement reused its nonce and socket owner.** The original navigation delegate rebuilt only `.reload`; `location.assign('zgasset://bundle/index.html')` and ordinary same-entry links could load a fresh document using the old user-script nonce, native owner and pending callback IDs. The Host now allows its first main navigation and genuine fragment-only changes, rebuilding all other replacement documents with a fresh bridge/owner.
3. **An old queued navigation callback could resurrect a destroyed WebView.** The asynchronous rebuild originally captured only self, so shutdown/background or another reentry before its dispatch could be undone by an old callback. The implementation now weakly captures the old WebView, checks `self.webView === oldWebView`, and refuses rebuilding in background.
4. The owner's already-known pending UIAlert completion issue has been corrected with a once-only completion and unified cancellation/dismissal in shutdown. This review did not duplicate or overwrite that work.

`host-wire-review.cjs` executes the actual iOS JS shim, captures real outgoing storage/report messages, checks their fields against the current Swift dispatch and verifies the relevant current navigation/teardown source gates. It passes. It does not execute Swift; navigation-runtime race acceptance remains a Mac check.

## Other reviewed boundaries

- Main-frame, local `zgasset://bundle/index.html` and document nonce checks restrict bridge admission. Weak script handlers avoid a WKUserContentController→bridge→WebView ownership cycle. Invalidation clears pending IDs and detaches the WebView; completion and emit both check active state, preventing old network/storage callbacks from painting a replacement.
- Network responses preserve JSON-string `body` and status/code/delivery metadata expected by the actual r119 nativeReply. The final Host explicitly maps reply dictionaries to `Any`, matching its Result type. Request IDs reject CFBoolean values and accept only integral values within the JavaScript safe range. No additional obvious Swift5 type blocker was found in the reviewed sources, but only Xcode can confirm compilation.
- Keychain uses a private service/account namespace, non-synchronizing generic passwords, `WhenUnlockedThisDeviceOnly`, checked update/add/delete results and SecRandomCopyBytes. Lease persistence allowlists the same fields as Android and drops login password, normal bearer token and unrelated fields. Numeric lease behavior intentionally matches the existing Android sanitizer. Device credentials persist across lease forgetting; ordinary game passwords are not saved.
- Storage operations share one serial queue and return on main. Accepted durable storage operations may finish after document invalidation, while the obsolete callback is discarded; serial ordering ensures a later record/forget request follows earlier writes. This is distinct from replaying an unknown game mutation.
- App background/process termination/navigation failure all shut down the page; asset cancellation cancels current operations without permanently invalidating the reusable scheme handler. Existing source cancellation-token identity guards prevent old asset-worker delivery into new URL tasks.

## Native acceptance added for macOS/iOS Simulator

`ios/ZhanguoTests/Host/ZGHostContractTests.swift` contains:

- Local-document URL allowlist tests.
- Isolated disposable Keychain service: record/read across two storage instances, sensitive-field filtering, stable generated device identity, lease forgetting.
- Actual Apple WKWebView + actual bundled `native-ios.js` + actual Host bridge + Keychain roundtrip, including the outgoing `body` fields and performance report collection. This test uses a tiny in-memory local scheme page and never calls the network.

The app/asset NativeConfig need not point at a test server: the WK test's networking service is instantiated but no TCP route is invoked. Test Keychain services use random test-only names and are deleted afterwards. Capability values are not printed.

Still required on a Mac: compile all Swift/XCTest, run the above native test, and verify the full navigation race (hold an old callback, replace the entry document, confirm a new nonce and no old reply; schedule a rebuild then shutdown/background before dispatch, confirm no page resurrection). `host-wire-review.json` records the source hashes and explicitly marks the missing Apple runtime validation.

## Final transport integration recheck

The independently reviewed generated iOS overlay is `client.js` SHA256 `29853289ab75fc2d1abe43021365bdb62aadaf6243d82d73b75b558843d08370` (18,358,623 bytes). The actual `ios/tests/js/test-contract.cjs` was rerun independently after the transport owner fixed a concrete asynchronous decode race. Its result is recorded in `ios/tests/js/contract-report.json` and passed.

Originally, a second native poll could call the same `FrameCodec.push` while the previous push was awaiting decompression. The final transport serializes pushes through a queue owned by each connection generation. Reconnecting creates a new queue; queued old work checks its old generation and connection ID before touching the decoder, and an old in-progress push checks them again before delivering each packet. The executable fixture holds the first push, confirms the second push has not entered, closes/reconnects, releases the first push and confirms zero stale packet deliveries and no second old push. A subsequent real compressed/encrypted codec packet on the new connection still decodes correctly.

The actual generated transport and bridge contract tests also cover stale open/receive/send completion isolation, explicit per-instance IDs on all send/receive/close operations, JSON-string reply bodies, strict storage reply types, disposal, native error propagation, GBK encoding and RC5/DES framing. Native TCP callbacks are mocked in these Node tests; this successful result does not replace the pending Apple Network.framework/WKWebView/Keychain XCTest execution.
