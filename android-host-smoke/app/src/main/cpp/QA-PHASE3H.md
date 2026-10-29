# QA Phase 3H — First Actor Render

- Audit date: 2026-10-28
- Scope: `MainActivity.java`, commit `0736b46`, existing `view51.js`/`client.js`, device render state
- Mode: read-only product source; independent Gradle debug build; MuMu `emulator-5554` execution and WebView DevTools query; no product/tests/Java/JS changes; no push.
- MainActivity SHA-256: `2bfb2c5daa00247cea34b1abae53134b1c63f249fc6a0e56af0c409309eba068` (13,090 bytes) — matches the task identity.

## 1. Verdict

**PASS for the first actor render and scene membership.**

`objects=1`, `mob1001=true`, and the actor mesh is actually in `sceneQA.scene.children` (not merely in the object map). The render loop is alive at `calls=8`; the stable DevTools sample was `fps=60`, while the 1s console line showed the early value `fps=51`. No `sceneQA=missing`, `sceneQA-error`, or uncaught error was found, and the old UI loaded. No actor/scene-membership failure.

## 2. URL and GameView ownership

- `MainActivity.java:176` loads `index.html?sceneQA=1`.
- `view51.js:662` defines `GameView`; `view51.js:682` assigns `window.sceneQA=this` when the query parameter is present.
- The adapter at `MainActivity.java:76-83` waits for that real `window.sceneQA`; it does not fabricate a replacement object.

## 3. Actor state and scene membership

The adapter supplies:

```text
mobs:[{id:1001,name:'A',x:3,z:4,y:0,hp:10,maxHp:10,hpKnown:true,dead:false,alive:true,kind:2}]
```

and calls `q.update(s)`. The log expression checks:

```text
mob = q.objects.get('mob1001')
inScene = mob && mob.mesh && q.scene.children.indexOf(mob.mesh) >= 0
```

So `mob1001=true` means the mesh is in the actual scene child list, not only a map entry.

## 4. Device results

### 1s console line

Actual logcat after install/start:

```text
I/ZG335JS: ZG335JS sceneQA=1 player=T objects=1 mob1001=true calls=8 fps=51
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html?sceneQA=1
```

At the first sample (adapter timer is 1000 ms), the actor and call count were already correct; fps was 51 during shader warm-up.

### Stable sample after ≥3s

The app was left running and queried through WebView DevTools (page URL `index.html?sceneQA=1`). At that later stable point:

```json
{"scene":true,"objects":1,"mob":true,"inScene":true,"children":6,"calls":8,"fps":60,
 "url":"https://appassets.androidplatform.net/assets/index.html?sceneQA=1"}
```

The stable result confirms:

- `objects=1`
- `mob1001` exists
- its mesh is in `scene.children`
- `calls=8`
- `fps=60` (the task's illustrative 54 is device-timing-dependent; stable fps is >0 and the loop is active)

## 5. Error and legacy UI checks

A full log filter found no:

- `ZG335JS sceneQA=missing`
- `ZG335JS sceneQA-error`
- `Uncaught` errors

The old UI loaded at `https://appassets.androidplatform.net/assets/index.html?sceneQA=1`.

## 6. Modified-file check

`git show --stat 0736b46`:

```text
.../app/src/main/cpp/PHASE3H-REPORT.md             | 64 ++++++++++++++++++++++
.../src/main/java/com/zg335/host/MainActivity.java |  4 +-
2 files changed, 66 insertions(+), 2 deletions(-)
```

`view51.js`, `client.js`, the protocol parser and the 288-byte record layer were not modified by T65. The adapter does not inject a fake `THREE`; vendor modules remain served through the packed fallback and were 200 in the same run.

## 7. Release hygiene

Current release APK:

- SHA-256: `4f328af7e3a844c682f0df51031f5e7f1a46485c55543b8bf99196780758cb2e`
- Zip scan `keystore|zg_test|.p12|.jks`: **0 matches**

## 8. Findings summary

| Item | Result |
|---|---|
| `index.html?sceneQA=1` | PASS |
| window.sceneQA from real view51 GameView | PASS |
| objects=1 | PASS |
| mob1001 exists | PASS |
| actor mesh in scene.children | PASS: inScene=true, children=6 |
| calls after warm-up | PASS: 8 |
| stable fps after ≥3s | PASS: 60 (1s sample 51) |
| sceneQA missing/error/Uncaught | PASS: none |
| old UI loaded | PASS |
| view51/client/protocol/record layer unchanged | PASS: commit scope |
| release p12/keystore | PASS: 0 |

## 9. Boundary

This is local WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, whole-game rendering, or game-field semantics. No product source, tests, Java or JS was modified; no push was performed.
