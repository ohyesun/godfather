# Binary Shadow → view51 Phase 3I Report

Task: T67. Replaces the hardcoded sceneQA state with a shadow built from the
288B FULL packet (`/zg/render`), then applies it once to `view51.GameView`.
No view51/client.js/protocol/record-layer changes.

## Binary shadow mapping

The adapter parses the envelope and all 7 records:

- MAP `0x0101` → `map.id`
- SELF `0x0102` → `player` (uid from record entity id, name/x/y/z/level/dead;
  `hpKnown:false` because SELF has no HP; `alive` derived from `dead`;
  profession/sex/dir/moveSpeed only when present)
- ACTOR_UPSERT `0x0103` → actor map by entity id
- ACTOR_REMOVE `0x0104` → delete by entity id
- EVENT `0x0105` → event array (`event_id`, type mapped from enum)
- HUD `0x0106` → retained HUD object (`gold`, `vipLevel`, `level`)

ACTOR 1001 is now emitted as `actor_kind=2` (mob); 1002 is `kind=1` and is
removed by the following REMOVE. Final shadow actors = 1001 only.

No hardcoded `1001`/`A`/`3`/`4` state assignments remain in MainActivity.

## Device result

MuMu `emulator-5554`, WebView `110.0.5481.154.1`:

```text
I ZG335JS : ZG335JS full=288 records=7 actors=1 events=1 hud=1 map=1 self=T
I ZG335JS : ZG335JS sceneQA-binary map=1 self=T actors=1 mob=1001 objects=1
```

- map from MAP: 1
- self from SELF: T
- actors after remove: 1
- first mob: 1001
- GameView objects: 1
- No `sceneQA-error`, `sceneQA=missing`, or `page-error`.

Object creation is real: the mob came from binary actor kind 2 and the object
map count is 1. The old hardcoded actor state is gone.

## Tests / APK

C tests remain green:

```text
BRIDGE-ABI PASS
FRAME-PASS tokens=5 keys=2
RENDER-PASS records=7 bytes=288 actors=2 remove=1 events=1 hud=1
```

APK and release:

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,845,072 | `4fceb7d09f2acf196c7607f00b776295584282c3f3cb3ad52f964fdf1e2d8624` |
| `app-release-unsigned.apk` | 54,773,570 | `cbfd53beef842210d445cfed16e6f8d8f3f49bd297704c6da7d53b0093823f78` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `app/src/main/java/com/zg335/host/MainActivity.java` | 14397 | `cd4ea6d09b33a1e57596578154cc8ba98346546b8e99462a5fcac1c3a36ff4dd` |
| `bridge/src/zg_bridge.c` | 29450 | `bb57447bda3c4ecb1c312ee3e71561cfd05fa601a41ba5b9b8dc59ec8f53af78` |
| `bridge/tests/test_render.c` | 5264 | `45087e8586caea0f486203d2fdbbdff43b7780dc5705c06a53d48cdee22195aa` |

No view51/client.js/protocol/record-layer changes; no push.
