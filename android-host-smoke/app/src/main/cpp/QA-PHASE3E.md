# QA Phase 3E — 288-byte Seven-Record FULL

- Audit date: 2026-10-28
- Scope: `bridge/src/zg_bridge.c`, `bridge/tests/test_render.c`, `MainActivity.java`, JNI render path
- Mode: read-only product source; independent NDK r27 build; MuMu `emulator-5554` execution; no product/tests/Java/JS changes; no push.
- Frozen identities verified:
  - source `afd1ced4c8557a0a4278dbf56c8cce511cd3c0c4c765cd3986b4eedef461013b` (29,459 bytes)
  - test_render `a383e3ed144b915a8850199da76edc6144feec2df96a7aa0730c472310740afb` (5,264 bytes)
  - MainActivity `b2b44f664948c6dbd0d84d45893a171bc174a7a49c0fd287f253ba6130e0682a` (11,581 bytes)

## 1. Verdict

**PASS for the 288-byte seven-record FULL stream and JS shadow.**

The actual packet is 288 bytes with `payload_bytes=264` and `record_count=7`; MAP, SELF, two ACTOR_UPSERT records, ACTOR_REMOVE, EVENT and HUD are all laid out at the required offsets. The JS shadow reports final `actors=1` after the 1002 remove, plus `events=1`, `hud=1`, `map=1`, `self=T`.

## 2. Byte/arithmetic layout

| Range | Bytes | Record | Check |
|---|---:|---|---|
| 0-23 | 24 | envelope | kind FULL, header24, payload264 (`0x108`), record_count7, frame_id1, base0 |
| 24-45 | 22 | MAP | type `0x0101`, entity0, payload10, field `0x0100` u32=1 |
| 46-116 | 71 | SELF | type `0x0102`, entity1, payload59; name/x/z/visual/dir/level/dead |
| 117-162 | 46 | ACTOR_UPSERT 1001 | type `0x0103`, entity1001, payload34; kind/name/x/z |
| 163-208 | 46 | ACTOR_UPSERT 1002 | type `0x0103`, entity1002, payload34; kind/name/x/z |
| 209-220 | 12 | ACTOR_REMOVE 1002 | type `0x0104`, entity1002, payload0 |
| 221-249 | 29 | EVENT | type `0x0105`, entity0, payload17; event_id1 u32 + type1 u8 |
| 250-287 | 38 | HUD | type `0x0106`, entity0, payload26; gold123 i32 + vip2 u16 + level3 u16 |

Independent arithmetic:

```text
24 + 22 + 71 + 46 + 46 + 12 + 29 + 38 = 288
payload = 288 - 24 = 264
```

Actor record 46 = header12 + kind7 + name7 + x10 + z10. EVENT 29 = header12 + event_id10 + event_type7. HUD 38 = header12 + gold10 + vip8 + level8. All TLV widths match the schema.

## 3. Source order and fields

C `write_actor_upsert()` emits actor_kind 0x0001, name 0x0002, x 0x0010, z 0x0012. It writes ACTOR1001 at offset117, ACTOR1002 at offset163, then `write_record_header(dst+209, 0x0104, 1002, 0)`; the remove is therefore after the 1002 upsert. The test checks entity IDs 1001/1002, actor payload34, remove entity1002/payload0, EVENT entity0/payload17, HUD entity0/payload26, and the values `A(3,4)` / `B(5,6)`, event_id1/type1, gold123/vip2/level3.

## 4. Independent execution

NDK r27 clang 18.0.4 build for `x86_64-linux-android24`, executed on `emulator-5554`:

```text
RENDER-PASS records=7 bytes=288 actors=2 remove=1 events=1 hud=1
EXIT=0
```

The C test also checks cap287 -> OVERSIZE, no-pending -> AGAIN, second request frame_id2, and render_packets1/2.

## 5. JS shadow

`MainActivity.java:38-71` uses only `Uint8Array`, `DataView`/little-endian helpers and the TLV `fields()` helper:

- `actors` is a `Map` keyed by record header entity_id.
- ACTOR_UPSERT `0x0103` sets kind/name/x/z.
- ACTOR_REMOVE `0x0104` deletes the entity.
- EVENT `0x0105` increments events.
- HUD `0x0106` increments hud.
- MAP/SELF populate map/self.

Actual MuMu logcat:

```text
I/ZG335JS: ZG335JS full=288 records=7 actors=1 events=1 hud=1 map=1 self=T
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
```

Final `actors=1` proves the 1002 remove was applied after its upsert. No JSON/base64/hex/`evaluateJavascript` frame path is present.

## 6. Record/code hygiene and release

- New C/JS path contains no `0x7F01`; the old `client.js` asset is not used as a new record path.
- `MainActivity` does not call `view51`, Three.js or `client.js` for this record path.
- Current release APK `app-release-unsigned.apk`:
  - SHA-256: `52e5f4742e1cee50d8957f02d4e0e6101bd3a94762af60d616e6dd7ccc0145f0`
  - Zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
  - Dex contains `if(v.length!==288)`, `actors.delete(entity)`, no `0x7F01`.
- Current debug APK SHA-256: `ebd718d4f84440face1e5fbedafbe0286b23d4b1315cad0a7c2fa3a7b120f313`.

## 7. Findings summary

| Item | Result |
|---|---|
| 288B / 7 records | PASS |
| payload_bytes264 / record_count7 | PASS |
| MAP22 / SELF71 / ACTOR46×2 / REMOVE12 / EVENT29 / HUD38 | PASS |
| ACTOR required kind/name/x/z | PASS |
| 1002 upsert then remove order | PASS: final shadow actors1 |
| EVENT/HUD entity0 + field widths | PASS |
| JS Uint8Array/DataView shadow | PASS |
| JSON/base64/hex/evaluateJavascript | PASS: none |
| new path `0x7F01` | PASS: absent |
| release p12/keystore | PASS: 0 |
| view51/Three/client.js new path | PASS: not used |

## 8. Boundary

This is local C/JNI/WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, game-field semantics, client.js/Three.js integration, or whole-product acceptance. No product source, tests, Java or JS was modified; no push was performed.
