# MAP + SELF + ACTOR/EVENT/HUD FULL Report

Task: T59. Extends the 117B MAP+SELF fixture with two ACTOR_UPSERT records,
one ACTOR_REMOVE, one EVENT and one HUD record. Only schema-defined fields are
used. No view51/Three.js/client.js integration.

## Exact size calculation

| Record | Length |
|---|---:|
| envelope | 24 |
| MAP | 22 |
| SELF | 71 |
| ACTOR_UPSERT (1001) | 46 |
| ACTOR_UPSERT (1002) | 46 |
| ACTOR_REMOVE (1002) | 12 |
| EVENT | 29 |
| HUD | 38 |
| **total** | **288** |

- Envelope: kind FULL, payload_bytes **264** (all records after envelope,
  including 12B record headers), record_count **7**.
- The task text said “record_count=6” but enumerated 7 records; the actual wire
  value is 7, asserted by both C and JS.
- ACTOR records use only `actor_kind=1`, `name`, `x`, `z` (34B payload each).
  Values:
  - 1001 / A / x=3 / z=4
  - 1002 / B / x=5 / z=6
- ACTOR_REMOVE entity_id=1002, payload 0. JS applies both upserts then the
  remove, leaving `actors=1`.
- EVENT: event_id=1, event_type=1 (impact), header entity_id=0.
- HUD: gold=123, vip_level=2, level=3, entity_id=0.

## C tests

```text
BRIDGE-ABI PASS
ABI_EXIT=0
FRAME-PASS tokens=5 keys=2
FRAME_EXIT=0
RENDER-PASS records=7 bytes=288 actors=2 remove=1 events=1 hud=1
RENDER_EXIT=0
```

`test_render.c` asserts each record type/entity/payload and the key
ACTOR/EVENT/HUD fields. Capacity 287 returns OVERSIZE; 288 succeeds.

## JS shadow parse

The AssetLoader `/zg/render` endpoint returns the 288-byte octet-stream. JS
uses a TLV parser with unknown-field skipping and maintains a shadow actor map.

```text
I ZG335JS : ZG335JS full=288 records=7 actors=1 events=1 hud=1 map=1 self=T
```

The legacy UI loaded:

```text
I ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
```

No JSON/base64/hex/evaluateJavascript data path and no view51/Three.js changes.

## APK

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 44,490,944 | `ebd718d4f84440face1e5fbedafbe0286b23d4b1315cad0a7c2fa3a7b120f313` |
| `app-release-unsigned.apk` | 43,151,074 | `52e5f4742e1cee50d8957f02d4e0e6101bd3a94762af60d616e6dd7ccc0145f0` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source/test hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `bridge/src/zg_bridge.c` | 29459 | `afd1ced4c8557a0a4278dbf56c8cce511cd3c0c4c765cd3986b4eedef461013b` |
| `bridge/tests/test_render.c` | 5264 | `a383e3ed144b915a8850199da76edc6144feec2df96a7aa0730c472310740afb` |
| `app/src/main/java/com/zg335/host/MainActivity.java` | 11581 | `b2b44f664948c6dbd0d84d45893a171bc174a7a49c0fd287f253ba6130e0682a` |

No Phase0 ABI/parser semantics, old Java/assets, client.js or Three.js changes;
no push.
