# QA Phase 3J — Real Shadow 10 NPC Render

- Audit date: 2026-10-28
- Scope: `bridge/src/zg_bridge.c`, `bridge/tests/test_render.c`, `MainActivity.java`, commit `b4d85ff`, MuMu scene keys
- Mode: read-only product source; independent NDK/Gradle build; MuMu `emulator-5554` execution and WebView DevTools query; no product/tests/Java/JS changes; no push.
- Frozen identities verified:
  - source `2d096f81eab844094aa32764ba51c8911d6e80a0ce2deff3b019fee7688fd1cc` (30,257 bytes)
  - test_render `513fbacf025b89ea9b061737c7a328eec05eee8a1b8999c5874849074a27bdac` (7,207 bytes)
  - MainActivity `2032f91ea31f295eb6147ac37d9b50f798a25f5b3f2c8a1f83a0c136b4df0d23` (14,614 bytes)

## 1. Verdict

**PASS for the 10-unique-NPC real-shadow render.**

The fixture produces exactly 10 unique actor IDs, no 99580 and no synthetic IDs. The actual packet is 829 bytes / 12 records / payload805. The JS shadow reports `actors=10 objects=10` and `real-keys=10 no-extra=true`; the actual `sceneQA.objects` key set contains exactly the ten `npc*` keys including `npc99579`, with no `99580`.

## 2. Fixture IDs and records

`zg_bridge.c` writes these 10 actor IDs in order:

```text
792, 791, 803, 800, 799, 773, 801, 772, 769, 99579
```

- Total: 10 IDs
- Unique: 10
- Expected order: exact match
- 99580: absent
- 99579: one occurrence, last actor snapshot

The packet contains MAP + SELF + 10 ACTOR records = **12 records**. `test_render.c` asserts `PACKET_BYTES=829`, `record_count=12`, and `payload_bytes=805`; it also walks all 10 actor records and checks entity IDs, names, kind, x/z, and level. The actor array in the test has exactly the same ten IDs.

## 3. Independent C test

Built with NDK r27 clang 18.0.4 and run on `emulator-5554`:

```text
BRIDGE-ABI PASS
FRAME-PASS tokens=5 keys=2
RENDER-PASS real map=2 self=<real name> npc=10 records=12 bytes=829
ABI_EXIT=0 FRAME_EXIT=0 RENDER_EXIT=0
```

The console display mangles the Chinese name because of terminal encoding, but the byte-level test asserts UTF-8 string fields and all IDs.

## 4. Real device log and key set

Actual logcat after installing the T70 debug APK:

```text
I/ZG335JS: ZG335JS full=829 records=12 actors=10 events=0 hud=0 map=2 self=<real name>
I/ZG335JS: ZG335JS real map=2 self=<real name> actors=10 objects=10
I/ZG335JS: ZG335JS real-keys=10 no-extra=true
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html?sceneQA=1
```

Stable WebView DevTools key query:

```json
{"objects":10,
 "keys":["npc792","npc791","npc803","npc800","npc799","npc773","npc801","npc772","npc769","npc99579"],
 "has99580":false,
 "has99579":true,
 "url":"https://appassets.androidplatform.net/assets/index.html?sceneQA=1"}
```

This proves the objects are the real GameView actor keys, not just a count. No `sceneQA=missing`, `sceneQA-error`, `page-error`, or `Uncaught` was found in the log.

## 5. Scope / release

`git show --stat b4d85ff` changed `MainActivity.java`, `bridge/src/zg_bridge.c`, `bridge/tests/test_render.c` and the report; `view51.js`, `client.js`, and the old assets were not modified. The adapter/build still uses the packed vendor fallback and the existing 288/829 binary path.

Current release APK:

- SHA-256: `3c9f28460ab39a8ee5c5d718d74ab6d93e9d232a2fcb9aadd2a86b77500aefae`
- Zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- Release dex contains the real-key/829 path and no `99580`.

## 6. Findings summary

| Item | Result |
|---|---|
| 10 unique NPC IDs | PASS |
| 99579 only latest snapshot | PASS: one occurrence |
| No 99580/synthetic ID | PASS |
| 829B / 12 records / payload805 | PASS |
| entity IDs unique | PASS: 10 unique |
| JS full=829 records=12 actors=10 objects=10 | PASS |
| real-keys=10 no-extra=true | PASS |
| sceneQA key set = 10, no 99580 | PASS |
| no sceneQA error/missing/page-error | PASS |
| view51/client/protocol/assets unchanged | PASS: commit scope |
| release p12/keystore | PASS: 0 |

## 7. Non-blocking concern

`zg_bridge.c:893` comment says “11 real NPC ACTOR_UPSERT records,” while the code actually writes 10 actor upserts and the test/device counts are 10. This is a stale comment, not an extra record or object. It should be corrected for clarity.

## 8. Boundary

This is local C/WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, whole-game rendering, or game-field semantics. No product source, tests, Java or JS was modified; no push was performed.
