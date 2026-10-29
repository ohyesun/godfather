# QA Phase 3D — MAP + SELF 117-byte Record

- Audit date: 2026-10-28
- Scope: `bridge/src/zg_bridge.c`, `bridge/tests/test_render.c`, `MainActivity.java`, JNI render path
- Mode: read-only product source; independent NDK r27 build; MuMu `emulator-5554` execution; no product/tests/Java/JS changes; no push.
- Frozen identities verified:
  - source `1ea3e1beda82897189b5475c11337429677608cc13c120274791ca863899a410` (27,125 bytes)
  - test_render `677850c847f407dbd56717f8d003a11f451af07276df0c5589adc7e9ae2f912f` (3,721 bytes)
  - MainActivity `43d80089fbc4a417dae4be0e7bda962227ceb3daa470d905851c85dc52d02528` (11,202 bytes)

## 1. Verdict

**PASS for the 117-byte MAP+SELF record path.**

The 24-byte FULL envelope with `record_count=2` and `payload_bytes=93`, the 22-byte MAP record and the 71-byte SELF record are emitted and independently parsed on MuMu. The new C/JS path contains no `0x7F01`, and the release APK has no p12/keystore material.

## 2. Byte-level layout

C `zg_bridge.c:746-837` and test `test_render.c`:

| Range | Bytes | Content |
|---|---:|---|
| 0-23 | 24 | envelope: magic `47 5A`, version1, kind FULL=1, header24, frame_id1, base0, payload93 (`0x5D`), record_count2, reserved0 |
| 24-45 | 22 | MAP record: type `0x0101`, schema0, entity0, payload10; field `0x0100` u32 value1 |
| 46-116 | 71 | SELF record: type `0x0102`, schema0, entity1, payload59 (`0x3B`); SELF TLVs |

SELF TLVs:

```text
name          0x0002 type6 len1 "T"      7 bytes
x             0x0010 type5 len4 1.0f    10 bytes
z             0x0012 type5 len4 2.0f    10 bytes
visual_model  0x0003 type6 len1 "m"      7 bytes
dir_deg       0x0013 type5 len4 0.0f    10 bytes
level         0x0007 type2 len2 1       8 bytes
dead          0x0022 type1 len1 0        7 bytes
                                       ----
SELF payload                             59 bytes
```

Arithmetic: `24 + 12+10 + 12+59 = 117`. Envelope `payload_bytes = 10 + 59 + 12*2 = 93`; both document and test agree. The `test_render.c` checks offsets 24, 46, field offsets 36/40/42, 58/64, 65/71, 75/81, 85/91, 92/98, 102/108, 110/116.

## 3. Independent C execution

Built with NDK r27 clang 18.0.4 for `x86_64-linux-android24`, executed on `emulator-5554`:

```text
RENDER-PASS full=117 map=1 self=T
EXIT=0
```

The C test also checks cap `116` -> `OVERSIZE`, no pending -> `AGAIN`, frame_id1 then2, render_packets1 then2.

## 4. JS adapter

`MainActivity.java:38-65` parses only binary bytes:

- `Uint8Array` over `arrayBuffer()`
- `u16` / `u32` little-endian helpers
- `fields(v,start,end)` reads TLV field_id/type/value_len, skips unknown fields by length
- f32 through `DataView(...).getFloat32(0,true)`
- string through `TextDecoder().decode(v.subarray(p,p+len))`
- parses `record_count` at offset20, record type at offset24, MAP field `0x0100`, SELF fields, and logs:

```text
ZG335JS full=117 map=1 self=T x=1 z=2 dead=0
```

No JSON/base64/hex/`evaluateJavascript` frame path is present in MainActivity. `0x7F01` is absent from the new C/JS path and from the current debug/release dex.

## 5. MuMu execution

The T57 debug APK was installed on `emulator-5554`. Actual logcat:

```text
I/ZG335BRIDGE: webview=110.0.5481.154.1
I/ZG335BRIDGE: status=0 rx=24 frames=1 error=0
I/ZG335BRIDGE: frame status=0 rx=63 frames=2 error=0 before=24
I/ZG335JS: ZG335JS full=117 map=1 self=T x=1 z=2 dead=0
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
I/ZG335JS: ZG335JS full=117 map=1 self=T x=1 z=2 dead=0
...
```

This is real WebView console output, not a restatement of the author log. The legacy `index.html` remains loaded.

## 6. Release hygiene / old asset boundary

Current release artifact `app/build/outputs/apk/release/app-release-unsigned.apk`:

- SHA-256: `1ffd585249219907a2a270cd62c19a045106850260edd65a4b3698311df6fc03`
- Zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- New dex contains `ZG335JS full=`, `if(v.length!==117)`, `fields(v,off+12,end)`, and `new DataView(v.buffer`; no `0x7F01`.
- `client.js` is an old UI asset and is not used as a new record path; the new C/JS path is `MainActivity` plus the 117B render endpoint.

## 7. Findings summary

| Item | Result |
|---|---|
| 24B envelope kind FULL/record_count2/payload93 | PASS |
| MAP 22B (header12 + map_id TLV10) | PASS |
| SELF 71B (header12 + payload59) | PASS |
| record types/entity IDs/field IDs/widths | PASS |
| string u16 length + UTF-8 | PASS |
| total 117B | PASS: 24+22+71 |
| C test on MuMu | PASS: `RENDER-PASS full=117 map=1 self=T` |
| JS real log | PASS: `full=117 map=1 self=T x=1 z=2 dead=0` |
| JSON/base64/hex/evaluateJavascript | PASS: none |
| new path `0x7F01` | PASS: absent |
| release p12/keystore | PASS: 0 |
| old `client.js` asset | Out of new path; not used as acceptance |

## 8. Boundary

This is local C/JNI/WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, game-field semantics, client.js/Three.js integration, or whole-product acceptance. No product source, tests, Java or JS was modified; no push was performed.
