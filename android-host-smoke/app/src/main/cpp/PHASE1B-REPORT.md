# TLS Raw Byte Loopback Phase 1B Report

Task: T41. Scope: Java TLS plaintext transport and a debug-only self-contained
loopback into the C bridge. No game frame parsing, no JSON, no assets/client.js
or decompiled-Java changes.

## Files

- `app/src/main/java/com/zg335/host/TlsTransport.java`
  - `open(host, port)` with TLS handshake
  - one reader thread: read plaintext into a direct `ByteBuffer`, call
    `ZgBridge.feed(buffer, length, elapsedRealtime)`
  - one writer thread: writes `byte[]` only
  - `close()` closes socket/threads
  - `allowTestCertificate=true` is explicit test-only trust-all mode
- `app/src/debug/java/com/zg335/host/TlsLoopbackTest.java`
  - debug-only local `SSLServerSocket` on `127.0.0.1`
  - self-signed debug PKCS12 keystore
  - sends one 24-byte CONTROL frame
  - logs accumulated bridge stats
- `app/src/debug/res/raw/zg_test_keystore.p12`
  - debug-only test certificate/key; not packaged in release
  - test-only declaration: `app/src/debug/TEST-KEYSTORE.md`
- `app/src/main/java/com/zg335/host/MainActivity.java`
  - debug hook via reflection (test class is absent from release)
- `app/src/main/AndroidManifest.xml`
  - added `android.permission.INTERNET` required for local TLS sockets

## Debug-only certificate

The debug keystore was generated with JDK17 `keytool` as a self-signed RSA-2048
PKCS12 certificate for `127.0.0.1`, using legacy-compatible MAC/key protection.
It is a test asset under `src/debug/res/raw`, not a production credential and
not part of `assets/`.

Android rejected the first PKCS12 generation (`No installed provider supports
this key: PKCS12Key`); a legacy-algorithm PKCS12 then loaded. Android also does
not expose JKS, so JKS was discarded. The final debug path uses PKCS12.

Test-only declaration (`app/src/debug/TEST-KEYSTORE.md`):

- file: `zg_test_keystore.p12`
- alias `zgtls`, password `changeit`
- self-signed `CN=127.0.0.1`, SAN `IP:127.0.0.1`, RSA-2048
- no trust value; not a production credential or real trust anchor
- SHA-256 `574404a5bc8115280fdbc7b0bc7cf28b64a3630c5579d5fed6565edbcfa77cd9`
- only under `app/src/debug/res/raw`; release must exclude it
- no `zg_test_keystore.jks` is used: Android cannot load JKS. The release
  exclusion check covers `.jks`, `.p12`, `keystore`, and `zg_test` names.

## Build and device result

```text
./gradlew assembleDebug
BUILD SUCCESSFUL

adb install -r app-debug.apk
Success

adb shell am start -n com.zg335.host/.MainActivity
Starting: Intent { cmp=com.zg335.host/.MainActivity }
```

Logcat:

```text
I ZG335HOST: state=1 normal=0 overflow=0
I ZG335BRIDGE: status=0 rx=24 frames=1 error=0
I ZG335BRIDGE: tls status=0 rx=48 frames=2
```

`rx=48` = initial 24-byte control frame + 24-byte TLS-loopback control frame.
`frames=2`, `status=0` = `ZG_BRIDGE_OK`. No `ZG335BRIDGE` error lines in the
final run; process remained alive.

APK:

- Path: `app/build/outputs/apk/debug/app-debug.apk`
- Size: 43,989,024 bytes
- SHA-256: `f52802e993afba58cb53122c031bd505924f4bff82d0181e73b7ad5c8d1ad329`
- Native libs in APK:
  - `lib/x86_64/libzg335host.so` 28,320
  - `lib/arm64-v8a/libzg335host.so` 26,176

## Binary-only check

`TlsTransport` and `TlsLoopbackTest` code contains no `JSONObject`, `JSONArray`,
`JSON.parse` or `JSON.stringify`; the only "JSON" occurrences are comments. The
reader passes direct `ByteBuffer` bytes to C; the writer accepts `byte[]` only.
No frame contents are inspected in the test.

## Release keystore exclusion

`./gradlew assembleRelease` succeeded.

Debug APK zip entry:

```text
res/raw/zg_test_keystore.p12
```

Release APK (`app-release-unsigned.apk`, 42,794,686 bytes, SHA-256
`856226ce2266bfa06ccf4abe5b5cb892a09997265898842ba0fb397f71fe3eb7`)
zip scan for `keystore`, `zg_test`, `.p12`, `.jks`:

```text
keystore_hits []
```

Thus the debug test key is absent from release; no `zg_test_keystore.jks`
exists in either APK.

## Boundary

This proves TLS handshake, raw plaintext read, direct-ByteBuffer JNI feed, and
C bridge accumulation on the local emulator. It does not connect the real game
server, parse game commands, decrypt application frames, build render packets,
or validate production TLS pinning. The debug test certificate/trust-all path
must not be used in production. No push was performed.
