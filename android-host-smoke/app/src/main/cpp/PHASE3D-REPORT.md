# MAP + SELF 117B FULL Phase 3D Report

Task: T57. Implements the frozen `ZG-BINARY-RECORD-SCHEMA.md` minimum
MAP+SELF FULL frame and removes the `0x7F01` test record.

## Packet layout

Total: **117 bytes**.

| Offset | Size | Content |
|---:|---:|---|
| 0 | 24 | envelope: kind FULL, header 24, frame_id++, payload_bytes 93, record_count 2 |
| 24 | 12 | MAP record header: type `0x0101`, schema 0, entity_id 0, payload 10 |
| 36 | 10 | MAP `map_id` TLV: field `0x0100`, u32, value 1 |
| 46 | 12 | SELF record header: type `0x0102`, schema 0, entity_id 1, payload 59 |
| 58 | 7 | SELF `name` TLV: field `0x0002`, string `T` |
| 65 | 10 | SELF `x`: field `0x0010`, f32 1.0 |
| 75 | 10 | SELF `z`: field `0x0012`, f32 2.0 |
| 85 | 7 | SELF `visual_model`: field `0x0003`, string `m` |
| 92 | 10 | SELF `dir_deg`: field `0x0013`, f32 0.0 |
| 102 | 8 | SELF `level`: field `0x0007`, u16 1 |
| 110 | 7 | SELF `dead`: field `0x0022`, u8 0 |

`payload_bytes=93` is the complete record area after the envelope (MAP 22 +
SELF 71), consistent with the prior test-record semantics. All values are
little-endian; strings use the TLV `value_len` as a UTF-8 length prefix.

`0x7F01` generation, test assertions and JS parsing are removed.

## C/device tests

```text
BRIDGE-ABI PASS
ABI_EXIT=0
FRAME-PASS tokens=5 keys=2
FRAME_EXIT=0
RENDER-PASS full=117 map=1 self=T
RENDER_EXIT=0
```

`test_render.c` asserts 117 bytes, record_count 2, MAP type/map_id, SELF
entity_id/name/visual/x/z/dead, frame_id increments, and render_packets.

## JS / WebView

The `/zg/render` AssetLoader endpoint returns the 117-byte
`application/octet-stream`; JS parses the envelope and both TLV records.

```text
I ZG335JS : ZG335JS full=117 map=1 self=T x=1 z=2 dead=0
```

The legacy UI loaded:

```text
I ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
```

No ACTOR/EVENT/HUD, client.js, Three.js, JNI ABI or parser changes.

## APK

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 44,489,920 | `c3b9d9c49fe8c783ed28067685132f2b62eabcb890fbfeec7c103ad042a04f8c` |
| `app-release-unsigned.apk` | 43,150,722 | `1ffd585249219907a2a270cd62c19a045106850260edd65a4b3698311df6fc03` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected test-only)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source/test hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `bridge/src/zg_bridge.c` | 27125 | `1ea3e1beda82897189b5475c11337429677608cc13c120274791ca863899a410` |
| `bridge/tests/test_render.c` | 3721 | `677850c847f407dbd56717f8d003a11f451af07276df0c5589adc7e9ae2f912f` |
| `app/src/main/java/com/zg335/host/MainActivity.java` | 11202 | `43d80089fbc4a417dae4be0e7bda962227ceb3daa470d905851c85dc52d02528` |

Schema input:
`ZG-BINARY-RECORD-SCHEMA.md` SHA-256
`63410f3f3dff9a4fe676c0029266cff0331af5a9b4eea3b4153eaee9ddfd13fd`.

No push.
