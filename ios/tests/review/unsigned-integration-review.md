# Independent unsigned iPhoneOS packaging review

2026-10-16. Read-only review of Host/Bridge/Storage/Networking Swift source, Release project settings, and the new `build_unsigned_ipa.sh` / `package_unsigned_ipa.py`. The only execution was the explicit synthetic Python fixture suite on Windows. No Swift, Xcode, Mac shell, device, game account, live TCP or upload was used.

No additional obvious Swift compilation blocker was found in the reviewed non-asset source. Swift language mode is 5.0, strict concurrency checking is minimal, and Release uses whole-module optimization with `ONLY_ACTIVE_ARCH=NO`. UIKit/WebKit/Foundation/Network/Security and libz are linked. This is static review, not proof of SDK compilation. The separately assigned asset initializer correction was outside this review's ownership.

The reviewed Network API spellings match Apple's primary references: [`NWProtocolTCP.Options.noDelay`](https://developer.apple.com/documentation/network/nwprotocoltcp/options) is a Boolean property, and Apple's [local TLS identity example](https://developer.apple.com/documentation/network/creating-an-identity-for-local-network-tls) uses `sec_trust_copy_ref(trust).takeRetainedValue()`. Apple's [build settings reference](https://developer.apple.com/documentation/xcode/build-settings-reference) distinguishes Swift 5 concurrency diagnostics from Swift 6 language-mode errors. These references do not substitute for compiling against the selected SDK.

The unsigned shell selects actual `iphoneos`, generic iOS, Release, arm64, `SKIP_INSTALL=NO`, blank Team/identity and signing disabled. It uses a unique archive/build directory and does not invoke exportArchive or provisioning updates. It asks Xcode to embed required Swift support libraries and validates the resulting app before manual Payload ZIP creation. It cannot manufacture a valid Mach-O on Windows or bypass missing Apple SDKs.

The validator distinguishes arm64 iOS from arm64 Simulator/Catalyst with `LC_BUILD_VERSION`, validates bounded FAT/load-command structures, compares main executable/plist minimum OS, validates embedded native dependencies and resolves Swift system SDK stubs without requiring duplicate libswiftCore in the app. It preserves modes, streams large files, checks ZIP CRCs and refuses output overwrite. It labels the result unsigned and not installable without signing.

Two actionable boundaries were raised during this review and are now closed:

1. The original output test used only a lexical absolute path, while report output had no app exclusion. Canonical output and report targets are now checked before file creation; neither may write inside the input App, including `..` and parent-link resolution. New negative fixtures confirm the input App is not changed.
2. Embedded Mach-O minimum OS must not exceed the App deployment target. The owner added that check and a platform-iOS library requiring 17.0 inside a 16.4 App is rejected.

Final independent execution: `python ios/tools/test_unsigned_ipa.py` completed **11 tests: 10 passed, one skipped, no failures/errors**. The skipped case needs permission to create real symbolic links on Windows; the separate in-memory ZIP symlink mode/target test passed. Real filesystem symlink integration must still run on Mac. Fixture executable mode adaptation on NTFS is explicitly confined to tests.

Frozen source reviewed:

| File | SHA-256 |
|---|---|
| build_unsigned_ipa.sh | `416eeb6be2b489e7c4e2f9194f1a75afd38ed5e7296fddd01ef4a01bf126e1e4` |
| package_unsigned_ipa.py | `ecfa23a509794a3830eb0c805ce81ecfe2e957401c05720fad325e8afc00bdc9` |
| test_unsigned_ipa.py | `defaf98681865c7d5dce89f7e6c8abb9ac19d5b59e75b87d8ecb67138577a812` |

No remaining blocking issue was found in this bounded review. The retained machine-readable result is `tests/review/unsigned-ipa-offline.json`. No real device App/IPA has been produced or installed by this Windows execution; actual Xcode archive, Swift runtime embedding, Apple native tests, signing and iPhone runtime acceptance remain pending.
