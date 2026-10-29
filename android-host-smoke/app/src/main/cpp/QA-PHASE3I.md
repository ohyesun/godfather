# QA Phase 3I — Binary Shadow Render

- Audit date: 2026-10-28
- Scope: `MainActivity.java`, commit `43f5ab5`, ABI/FRAME/RENDER tests, device scene state
- Mode: read-only product source; independent NDK/Gradle build; MuMu `emulator-5554` execution and WebView DevTools query; no product/tests/Java/JS changes; no push.
- Frozen identities verified:
  - source `bb57447bda3c4ecb1c312ee3e71561cfd05fa601a41ba5b9b8dc59ec8f53af78` (29,450 bytes)
  - test_render `45087e8586caea0f486203d2fdbbdff43b7780dc5705c06a53d48cdee22195aa` (5,264 bytes)
  - MainActivity `cd4ea6d09b33a1e57596578154cc8ba98346546b8e99462a5fcac1c3a36ff4dd` (14,397 bytes)

## 1. Verdict

**PASS for binary-shadow state construction and actor rendering.**

The adapter parses the 288-byte FULL stream and constructs `GameView.update(s)` from the parsed MAP/SELF/ACTOR/EVENT/HUD data. There is no hardcoded `1001/A/3/4` actor state in MainActivity. The 1001 `kind=2` becomes a mob, the 1002 upsert is removed, SELF has `hpKnown:false`, and the device shows `objects=1`, `mob=1001`, with the mesh actually present in `scene.children`.

## 2. Adapter source evidence

`MainActivity.java`:

- Lines 67-76 parse the 288-byte packet:
  - MAP `0x0101` -> `map`
  - SELF `0x0102` -> `self`/`selfEntity`
  - ACTOR `0x0103` -> `{id,kind,name,x,z,...}` into `actors`
  - ACTOR_REMOVE `0x0104` -> `actors.delete(entity)`
  - EVENT `0x0105` -> `events[]`
  - HUD `0x0106` -> `hud`
- Line 77 stores the parsed shadow.
- Line 58 dispatches by `actor.kind`: `2` mobs, `1` players, `3` npcs, `4` drops.
- Line 59 builds `s` with `map/{id:shadow.map}`, SELF-derived `player`, parsed `mobs/players/npcs/drops`, parsed events and hud.
- Line 60 calls the real `q.update(s)` and reports `sceneQA-error` on exception.
- Line 56 constructs SELF with `hpKnown:false`; no HP is fabricated.

Exact grep counts in the adapter source:

```text
1001 = 0
hpKnown:false = 1
sceneQA-binary = 1
shadow.actors = 2
actors.delete(entity) = 1
```

The old hardcoded state (`1001/A/x:3/z:4`) is gone from MainActivity. The `1001` number in the device output comes from the binary ACTOR record, not an adapter literal.

## 3. Device log and stable state

ABI/FRAME/RENDER independently rebuilt and run on `emulator-5554`:

```text
BRIDGE-ABI PASS
FRAME-PASS tokens=5 keys=2
RENDER-PASS records=7 bytes=288 actors=2 remove=1 events=1 hud=1
```

Installed T67 debug APK and captured actual logcat:

```text
I/ZG335JS: ZG335JS full=288 records=7 actors=1 events=1 hud=1 map=1 self=T
I/ZG335JS: ZG335JS sceneQA-binary map=1 self=T actors=1 mob=1001 objects=1
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html?sceneQA=1
```

No `sceneQA-error`, `sceneQA=missing`, or page error appeared. Stable WebView DevTools reading after startup:

```json
{"objects":1,"mob":true,"inScene":true,"keys":["mob1001"],"calls":7,"fps":60,
 "url":"https://appassets.androidplatform.net/assets/index.html?sceneQA=1"}
```

`keys=["mob1001"]` and `inScene=true` confirm the actor is in the real GameView object map and scene, not just a local map entry.

## 4. Non-target files

`git show --stat 43f5ab5` changed `MainActivity.java`, `bridge/src/zg_bridge.c`, `bridge/tests/test_render.c` and the report. `view51.js`, `client.js`, and the protocol/record ownership boundaries were not modified by this commit. The adapter does not inject a fake `THREE`; vendor assets remain served via the packed fallback.

## 5. Release hygiene

Current release APK:

- SHA-256: `cbfd53beef842210d445cfed16e6f8d8f3f49bd297704c6da7d53b0093823f78`
- Zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- Release dex contains `sceneQA-binary` and no literal `1001` hardcoded state.

## 6. Findings summary

| Item | Result |
|---|---|
| Adapter parses 288B MAP/SELF/ACTOR/EVENT/HUD | PASS |
| No hardcoded 1001/A/3/4 state | PASS |
| actor_kind dispatch 2 -> mob | PASS |
| 1002 upsert then remove | PASS: actors=1 |
| SELF hpKnown:false / no fabricated HP | PASS |
| real `sceneQA-binary` log | PASS: map1/selfT/actors1/mob1001/objects1 |
| mesh in scene.children | PASS: stable inScene=true, keys=[mob1001] |
| no sceneQA error/missing/page-error | PASS |
| ABI/FRAME/RENDER | PASS |
| view51/client/protocol/record changes | PASS: not in T67 scope |
| release p12/keystore | PASS: 0 |

## 7. Boundary

This is local WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, whole-game rendering, or game-field semantics. No product source, tests, Java or JS was modified; no push was performed.
