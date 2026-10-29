# Final iOS integration review

2026-10-16. Read-only review of the completed source project, host/bridge, resource paths, generated Xcode project, scheme, Mac scripts and delivery statements. No business code or build settings were changed for this review. No device, account, launcher or game TCP connection was used.

**Result: no additional blocking integration defect found in the reviewed source. The deliverable is a source-and-resources ZIP, not a compiled or installable IPA.**

## Boundaries checked

- The Xcode project lists all 12 current Swift source files and four XCTest files. App and test source phases are separate; the test target depends on the app and has its hosted `TEST_HOST`/`BUNDLE_LOADER`. UIKit, WebKit, Foundation, Network, Security and libz appear in the linked framework references for both targets. Deployment target is 16.4; app Info.plist and host agree on iPhone/iPad landscape presentation.
- `GameAssets` and `GameOverlay` are folder resource references, preserving their contents beneath those exact bundle directory names. `NativeConfig.json` and LaunchScreen.storyboard are app resources. The host's `Bundle.main.resourceURL/GameAssets`, `GameOverlay`, and `GameOverlay/native-ios.js` paths match those references; configuration loading looks for the root-bundle `NativeConfig.json`.
- The entry URL is `zgasset://bundle/index.html`; relative game assets remain on the same scheme/host. The handler receives the separate overlay root; the original assets are not overwritten. The overlay index imports `ios-startup.js`, which checks real runtime capabilities before importing `client.js`.
- The host reads `GameOverlay/native-ios.js` as text and replaces its single **bare** `__ZG_IOS_BOOTSTRAP_JSON__` token with a JSON object literal, exactly matching the shim's `const bootstrap = ...` declaration. It installs one main-frame document-start WKUserScript, rather than serving an unresolved placeholder as an HTML script.
- `zgNative` channel, v1, current document nonce and main-frame/local-entry admission match the JS contract. Request95 fields and JSON-string reply bodies match. Storage and report dispatch both read the actual `body` field. Async storage remains a separate API; no synchronous legacy storage or uncertain game action is represented as successful. Per-connection UUIDs and per-generation queued frame decoding match the native transport contract.
- Navigation replacement creates a new bridge/owner, while stale queued navigation checks its old WebView identity and app background state. Shutdown invalidates bridge callbacks, closes native work, cancels assets and removes the old WebView. These are source-level findings; actual Apple lifecycle timing is still unverified.
- The shared TestAction explicitly provides `--zg-tests` with launch-argument inheritance disabled. AppDelegate checks that flag and the XCTest environment before constructing GameViewController. Written native tests use local/fake channels; the Simulator test command therefore does not intentionally cold-start the real game launcher.
- `check_on_mac.sh` and `export_ipa.sh` derive paths from their own `ios/tools` location and reject non-macOS. The former generates/verifies the project and invokes Simulator XCTest; the latter requires an explicit developer Team, verifies resources and produces timestamped archive/export paths. Neither script is evidence that an Apple build has already succeeded.
- README and `tests/acceptance-summary.json` accurately distinguish the implemented project and executed Windows checks from unexecuted Xcode, iOS Simulator, custom-scheme WK, iPhone, signing and IPA acceptance. They do not describe the source ZIP as an installable application. Store assets/submission are explicitly outside this stage.

## Executed evidence retained

The existing `tests/project-verification.json` records 1,781 verified direct resources, 82,344 packed records, 82,403 visible paths, four overlay files, 12 Swift sources and four tests, with native configuration present. Its `appleCompilationExecuted` and `iPhoneExecutionVerified` are explicitly false. This final review read that result and the generating/verification code; it did not redundantly rehash the 1.64 GB PAK.

The exact overlay reviewed is `client.js` SHA256 `29853289ab75fc2d1abe43021365bdb62aadaf6243d82d73b75b558843d08370`, with manifest SHA256 `12ec222299e51a0288079096e5e52acf14ab3f82e0c1356ac3bd8f4a7fe65daa`, based on untouched signed r119 APK `62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44`.

Previously executed `tests/js/contract-report.json` covers the real generated modules, original nativeReply, GBK/RC5/DES and connection/decode races. `tests/js/cold-start-report.json` covers actual overlay/browser startup at two sizes, seven dynamically parsed fixture zones, 193 verified resource requests, PAK PNG decode and visible missing-bridge failure. Native responses in those fixtures are mocked. `tests/review/host-bridge-review.md` records independent host/Keychain source review and the additional native tests written for Mac.

## Remaining acceptance

No Xcode or Swift compilation, Apple SDK linking, native XCTest, real WKWebView custom-scheme execution, physical iPhone login/gameplay/performance, Apple signing or IPA export has run in this Windows environment. The prepared tests and scripts are the next checks on a Mac. Passing Node/Edge and structural verification does not establish those Apple runtime results. No live account test was authorized or performed by this review.
