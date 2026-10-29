# QA Phase 6 — Host Real Scene / 18008 Resource Layer

- Audit date: 2026-10-28
- Scope: T77 HEAD `f385972`, `PHASE6-UI.png`, `PakRemote.java`, `PackedAssetsPathHandler.java`, current debug APK, MuMu scene state
- Mode: read-only source and device observation; old APK `com.zhanguo.chaptertwo` was force-stopped; no product/tests/Java/JS changes; no push.
- Current debug APK SHA-256: `cb6609845b6f55958fdecabae460438f2465c5ffe3cca35ddce503b63066376f`; installed size 55,839,725 bytes.
- Screenshot artifact SHA-256: `59ec5ed0741c50cad754b29e53e3f1f12a2939505cb00d991cc69c79ab29195b` (`PHASE6-UI.png`).

## 1. Verdict

**CONCERNS, not FAIL.**

The screenshot and the current device screenshot both show an actual loaded scene (咸阳, 烈火教父 神农26级, minimap 186,395, visible NPCs), not a login/loading page. The negative-offset 18008 path returns real resources and `MICRO-FAIL=0`. However, the independent full log gives `MICRO-OK=812` / total bytes `30,752,347`, not the author-reported `800` / `22,530,003`; the exact count claim is not independently reproduced. The 6020 observer has no `SCENE-OK` line; the scene is confirmed by screenshot only.

## 2. UI/scene evidence

Author artifact `PHASE6-UI.png` and a fresh device screenshot were inspected. Both show:

- location `咸阳`
- player `烈火教父 · 神农 · 26级`
- minimap coordinates `186,395`
- NPC labels such as `听泉寨山贼12级`, `听泉寨头目14级`, `听泉寨山贼`
- normal scene HUD/action bar, not a login or loading screen

This is actual scene entry, not a startup resource probe.

## 3. Resource-layer log

Independent full logcat from `emulator-5554`:

```text
MICRO-OK count = 812
MICRO-OK total bytes = 30,752,347
MICRO-FAIL = 0
pak hit = 10
pak miss negative = 810
```

The author report claims:

```text
MICRO-OK count = 800
MICRO-OK total bytes = 22,530,003
MICRO-FAIL = 0
```

The independent log includes actual path/byte records, for example:

```text
MICRO-OK path=native107/textures-report.json bytes=40
MICRO-OK path=native107/textures-report.json bytes=40 source=pak-remote
MICRO-OK path=scenes/xianyangcheng/map.json bytes=2709
MICRO-OK path=scenes/xianyangcheng/10_11.bin bytes=2165
MICRO-OK path=scenes/xianyangcheng/12_11.bin bytes=2264
MICRO-OK path=scenes/xianyangcheng/11_11.bin bytes=3416
MICRO-OK path=native107/textures/16786114.png bytes=178293
```

At least one required `scenes/xianyangcheng/map.json` and multiple `scenes/xianyangcheng/*.bin` paths are present. There is no `MICRO-FAIL` line in the independent log. `MICRO-FAIL` failures, when present, carry `path=` in `PakRemote.java`.

**CONCERN:** the exact author aggregate 800/22,530,003 is not reproduced. The current log has 812 entries / 30,752,347 bytes (793 unique paths / 30,707,153 bytes); this may reflect additional resource loads/duplicates after the initial scene, but I do not claim the author total as independently verified.

## 4. 6020 / SCENE-OK boundary

The independent log has:

```text
GAME-OK 6020 cmd=0a01 len=18 encrypted=des
SCENE-OK lines: 0
05/0c scene-success lines: 0
```

There is no `SCENE-OK` log. The UI screenshot is the evidence that the scene loaded. I am **not** claiming a `05/0c` or `SCENE-OK` packet was observed; the two layers are not conflated.

## 5. PakRemote / PackedAssetsPathHandler

Source:

- `PackedAssetsPathHandler.java:76-88`: negative offset/length enters `PakRemote.openRemote`; positive offset continues to local `resources107.pak` seek (`90-100`).
- `PakRemote.java:107-114`: remote `.js/.mjs/.html/.htm/.css/.svg` are rejected before cache/network.
- `PakRemote.java:46,86,89,92`: logs only `path`/`bytes` or `path` on failure; no token/content logging.
- `PakRemote.java:58-59`: config token is passed to `Zgmu` but not logged.

Actual logs show the negative path:

```text
ZG335ASSETS: pak miss negative scenes/xianyangcheng/map.json offset=-1
ZG335MICRO: MICRO-OK path=scenes/xianyangcheng/map.json bytes=2709
```

and local positive pak hits (`pak hit = 10`). This verifies negative-offset→18008 and positive-offset local routing.

No app-tag token/password/resource-content line was found; the only token occurrence in the full system log is an unrelated MuMu/adbd service token, not a ZG335 application log.

## 6. Release / unchanged files

- Current release APK SHA-256: `e5cfef6b63831e2a53364b935a1e9a83baeb9a5ad8912cc2b511b47ec2132af6`
- Zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- `view51.js` SHA `6fc9b01d...` and `client.js` SHA `04b5ed96...` remain unchanged; T77 changes are the Java resource layer plus the screenshot artifact.
- Old APK `com.zhanguo.chaptertwo` was force-stopped before the device observation.

## 7. Findings summary

| Item | Result |
|---|---|
| Actual UI scene, 咸阳 / 26级 / 186,395 / NPCs | PASS |
| Not login/loading page | PASS |
| Negative offset → 18008 real resources | PASS |
| Positive offset stays local pak | PASS |
| map.json + .bin paths logged | PASS |
| MICRO-FAIL=0 | PASS |
| Author MICRO-OK=800 / 22,530,003 | CONCERN: independent 812 / 30,752,347 |
| SCENE-OK / 05/0c | NOT OBSERVED; UI screenshot only |
| Remote js/css/html/svg blocked | PASS source |
| Token/password/resource content in app logs | PASS: none found |
| release p12 | PASS: 0 |
| view51/client/protocol/old assets unchanged | PASS |

## 8. Boundary

This is local WebView/socket/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, whole-game acceptance, or a `SCENE-OK` packet. No product source, tests, Java or JS was modified; no push was performed.
