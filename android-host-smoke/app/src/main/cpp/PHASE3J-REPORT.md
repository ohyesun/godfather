# Real Fixture Shadow Phase 3J Report (corrected)

Task: T70 correction. Feeds the desensitized real-session fixture into the
existing C→JNI→JS→view51 pipeline. No realtime server, protocol, record-layer,
view51/client.js or old asset changes.

## Fixture correction

Input: `E:/zg-handoff-workbuddy/real-corpus-20261029/fixture-map-self-npc.json`

The raw session contained 11 `05/07` snapshots but only **10 unique NPCs**.
`npcID=99579` appears twice because it is the same rabbit; the older snapshot
was removed and the newer one retained:

- npcID 99579, baseID 1901, name 兔子, x=186, y=395, dir=90, level=1,
  states=[3322806272], masterId=1

No synthetic `99580` or any synthetic entity id exists in the fixture, C, or
JS.

## Packet

- envelope 24
- MAP 22 (map_id=2)
- SELF 112 (payload 100; uid 1, 烈火教父, level 26, x=186/y=0/z=395,
  dir=135, hp=150/maxHp=997, profession=1)
- 9 full-name NPC ACTOR records ×68
- 1 兔子 ACTOR record ×59
- **total 829B**, payload_bytes 805, record_count **12**
- 10 ACTOR_UPSERT records, actor_kind=3, unique entity_id=npcID;
  name/x/z/level only; baseID/dir not written (not ACTOR schema fields)

## Tests / device

```text
BRIDGE-ABI PASS
FRAME-PASS tokens=5 keys=2
RENDER-PASS real map=2 self=烈火教父 npc=10 records=12 bytes=829
```

MuMu `emulator-5554`, WebView 110:

```text
ZG335JS full=829 records=12 actors=10 events=0 hud=0 map=2 self=烈火教父
ZG335JS real map=2 self=烈火教父 actors=10 objects=10
ZG335JS real-keys=10 no-extra=true
```

No `sceneQA-error`, `sceneQA=missing`, or `page-error`. The 10 unique NPCs
produce 10 GameView objects; no 99580 synthetic id is created.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,805,416 | `4e236d86ae0db437076710ce22002e54e518e9c6355f22068089ca5fa6be80ab` |
| `app-release-unsigned.apk` | 54,774,498 | `3c9f28460ab39a8ee5c5d718d74ab6d93e9d232a2fcb9aadd2a86b77500aefae` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source/test hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `app/src/main/java/com/zg335/host/MainActivity.java` | 14614 | `2032f91ea31f295eb6147ac37d9b50f798a25f5b3f2c8a1f83a0c136b4df0d23` |
| `bridge/src/zg_bridge.c` | 30257 | `2d096f81eab844094aa32764ba51c8911d6e80a0ce2deff3b019fee7688fd1cc` |
| `bridge/tests/test_render.c` | 7207 | `513fbacf025b89ea9b061737c7a328eec05eee8a1b8999c5874849074a27bdac` |

No pcap/account data, view51/client.js/protocol/record-layer changes; no push.
