# Binary Record Layer Phase 3C Report

Task: T53. Scope: end-to-end 40-byte FULL packet with one test-only 4-byte seq
record. No game fields, Three.js, client.js or legacy assets changed.

## C packet

`zg_bridge_request_full()` produces a 40-byte FULL packet:

| Range | Field | Value |
|---|---|---|
| 0..23 | 24B envelope | magic `0x5A47`, version 1, kind `1` FULL, keyframe flag, header 24, frame_id++, base 0, payload_bytes 16, record_count 1, reserved 0 |
| 24..35 | 12B record header | type `0x7F01`, schema 0, entity_id 0, payload_bytes 4 |
| 36..39 | 4B LE payload | seq++ |

`0x7F01` is **test-only** and is intended to be replaced by the PC contract
record later. `poll_render` requires capacity 40; short buffers return
`ZG_BRIDGE_OVERSIZE`; empty requests return `AGAIN`; success increments
`render_packets`.

## Tests

`tests/test_render.c` now asserts 40 bytes, envelope kind FULL, payload=16,
record_count=1, record type `0x7F01`, schema 0, entity 0, payload 4, seq 1,
second packet frame_id/seq 2, `render_packets=2`.

MuMu `emulator-5554`, `mumu-cli adb --vmindex 0`:

```text
BRIDGE-ABI PASS
ABI_EXIT=0
FRAME-PASS tokens=5 keys=2
FRAME_EXIT=0
RENDER-PASS records=1 type=7F01 seq=1
RENDER_EXIT=0
```

## JNI/Java/JS

- `pollRender` Java buffer raised to 64 bytes; accepts 40-byte packets.
- `WebViewAssetLoader` `/zg/render` returns `application/octet-stream`, or 204
  when no packet; no JSON/base64.
- JS adapter uses `fetch('/zg/render', {cache:'no-store'})` +
  `response.arrayBuffer()`, parses envelope and record, and logs through
  `WebChromeClient`.

First JS fetch on the new APK:

```text
I ZG335JS : ZG335JS records=1 type=7F01 seq=1 magic=5A47
```

Subsequent fetches increment seq (observed `seq=2`, `seq=3`, ...). The legacy UI
also loaded:

```text
I ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
```

## APK

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 44,489,152 | `2d90cc8a99b59c203230aeadc7f3698b3982a3ac1865dbbcc2f48141c5d2aa31` |
| `app-release-unsigned.apk` | 43,150,626 | `7e00f6abad85cb173dcb5ded387bb353b29b0bef82b05d06b05cca6055adba58` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: 0 hits for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source/test hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `bridge/src/zg_bridge.c` | 25362 | `c48d3603d0604cde8da81eec3999bbac5f7b4bc720b8e2c62d76f30e01c93fd6` |
| `bridge/tests/test_render.c` | 3446 | `4abdd512831990099ce67bd0dc2500f73c0198bd42aaa271dbedcab3152b6efa` |
| `app/src/main/java/com/zg335/host/MainActivity.java` | 10545 | `7d38f2ef8717cb76e60f291ca41402a986009f169acda2c35233c23decb16c2e` |

## Boundary

This is a local test-only record. It does not implement game fields, world
state, Three.js, production PC contract records, or real TLS transport. No push
was performed.
