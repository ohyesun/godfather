# QA Phase 3G — sceneQA Render Integration

- Audit date: 2026-10-28
- Scope: `MainActivity.java`, commit `12c716a`, existing `view51.js`/`client.js`
- Mode: read-only product source; independent Gradle debug build; MuMu `emulator-5554` execution; no product/tests/Java/JS changes; no push.
- MainActivity SHA-256: `dcf5fccb669877e7417ff35ed31fdfddea462dd3880f1e4ba37861b673f2c483` (12,881 bytes) — matches the task identity.

## 1. Verdict

**PASS for sceneQA ownership and render-loop execution; note actual fps was 55, not the illustrative 53.**

The URL is exactly `index.html?sceneQA=1`; `window.sceneQA` is set by the real `view51.js` `GameView` constructor, not by an adapter-created fake object. The adapter waits for that object and drives it with an explicit empty actor state, producing `objects=0` because players/mobs/npcs/drops are empty arrays. `calls=7` and `fps=55` prove the real render loop ran. No `sceneQA=missing` or `sceneQA-error` was emitted, and the old UI loaded.

## 2. URL and GameView ownership

- `MainActivity.java:176` loads `ASSET_ORIGIN + ASSET_INDEX + "?sceneQA=1"`, i.e. `https://appassets.androidplatform.net/assets/index.html?sceneQA=1`.
- `view51.js:662` defines `export class GameView`.
- `view51.js:682` has `if(new URL(location.href).searchParams.has('sceneQA'))window.sceneQA=this;`.
- The adapter at `MainActivity.java:76-83` reads `window.sceneQA` after loading; it does not assign a replacement sceneQA object and does not inject a fake `THREE`.

## 3. Adapter behavior and objects=0

The adapter waits for `window.sceneQA`, then constructs a test state with:

```text
players:[], mobs:[], npcs:[], drops:[], events:[]
player:{uid:1,name:'T',x:1,y:0,z:2,...}
map:{id:1}
```

It calls `q.update(s)` on the real GameView. `view51.js:816` iterates `players`, `mobs`, `npcs`, `drops`; all are empty, so the GameView object map stays empty. `objects=0` therefore means “no actors in the supplied test state,” not “render loop did not run.” The subsequent `drawCalls=7` and `fps=55` confirm the renderer ticked.

## 4. Real MuMu log

Installed the current T63 debug APK on `emulator-5554`. Actual logcat:

```text
I/ZG335BRIDGE: webview=110.0.5481.154.1
I/ZG335JS: ZG335JS asset vendor/build/three.module.js=200
I/ZG335JS: ZG335JS asset vendor/examples/jsm/loaders/GLTFLoader.js=200
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html?sceneQA=1
I/ZG335JS: ZG335JS sceneQA=1 player=T objects=0 calls=7 fps=55
```

The full log filter found **no** `ZG335JS sceneQA=missing` and **no** `ZG335JS sceneQA-error`. The page URL includes `?sceneQA=1`.

The task’s illustrative log says `fps=53`; the actual device run reported `fps=55`. The acceptance criterion is `fps>0` and `calls>0`; both passed. The numeric difference is timing/device variation, not a failure.

## 5. No modified legacy/render code

`git show --stat 12c716a`:

```text
.../app/src/main/cpp/PHASE3G-REPORT.md  | 62 ++++++++++++++++++++++
.../src/main/java/com/zg335/host/MainActivity.java | 11 +++-
2 files changed, 72 insertions(+), 1 deletion(-)
```

`view51.js`, `client.js`, the ABI/protocol parser and the 288-byte record layer are not in the commit. The adapter does not inject a fake `THREE`; vendor modules are loaded through the packed asset path and returned 200.

## 6. Release hygiene

Current release APK:

- SHA-256: `e1ca92e073f63d83bc542ba73cf39b3095ec826853aa15fcebd6c7b94f41a54a`
- Zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- Release dex contains `?sceneQA=1`, `ZG335JS sceneQA=1`, and `waitScene`.

## 7. Findings summary

| Item | Result |
|---|---|
| URL exactly `index.html?sceneQA=1` | PASS |
| `window.sceneQA` from view51 GameView | PASS: `view51.js:682` |
| adapter does not fake sceneQA/THREE | PASS |
| objects=0 explanation | PASS: empty players/mobs/npcs/drops |
| calls>0 / fps>0 render loop | PASS: calls7, fps55 |
| no sceneQA=missing/error | PASS |
| old UI loaded | PASS: `ui loaded ...?sceneQA=1` |
| view51/client/protocol/record layer unchanged | PASS: commit scope |
| release p12/keystore | PASS: 0 |

## 8. Boundary

This is local WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, full game rendering, or whole-product acceptance. No product source, tests, Java or JS was modified; no push was performed.
