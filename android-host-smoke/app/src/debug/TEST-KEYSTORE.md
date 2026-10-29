# Debug TLS Test Keystore

File: `zg_test_keystore.p12`

- Purpose: **debug-only TLS loopback test** for `TlsLoopbackTest`.
- Certificate: self-signed RSA-2048, `CN=127.0.0.1`, SAN `IP:127.0.0.1`.
- Alias: `zgtls`
- Store/key password: `changeit`
- Trust value: **none**. It is not a production trust anchor, pinned server
  certificate, signing key, or user credential.
- Source set: `app/src/debug/res/raw/` only. It is not present in main/assets
  and must not be packaged in release.
- SHA-256:
  `574404a5bc8115280fdbc7b0bc7cf28b64a3630c5579d5fed6565edbcfa77cd9`

`TlsLoopbackTest` uses explicit test-only trust-all on the client side. The
server keystore is loaded only to complete the local self-signed handshake.
Production code must use the real pinned certificate path instead.

## Why `.p12`, not `.jks`

JKS is not available on the Android runtime used for the test
(`KeyStoreException: JKS not found`). The first default PKCS12 generation also
used an algorithm Android rejected (`No installed provider supports this key:
PKCS12Key`). The final test file is legacy-compatible PKCS12 and is loaded as
`KeyStore.getInstance("PKCS12")`. No `zg_test_keystore.jks` is used or shipped.

## Release exclusion

`assembleDebug` includes this raw resource in the debug APK. `assembleRelease`
uses a different source set and must not contain this file or any path matching
`zg_test_keystore`, `*.p12`, or `*.jks`. The release APK zip listing is checked
in the T41 report.
