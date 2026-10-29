# view51 sceneQA Render Hook Phase 3G Report

Task: T63. Connects the 288B shadow state to `view51.GameView` through the
existing debug sceneQA entry. Only the MainActivity-injected script and URL
changed; `view51.js`, `client.js`, protocol and record layer were not modified.

## Implementation

- MainActivity loads:
  `https://appassets.androidplatform.net/assets/index.html?sceneQA=1`
- The injected adapter:
  1. polls `window.sceneQA` every 100ms for up to 8 seconds;
  2. logs `ZG335JS sceneQA=missing` if absent;
  3. builds the requested minimal state (generation 1, map.id 1, self T at
     x=1/z=2, empty actor/event lists);
  4. calls `sceneQA.update(s)`;
  5. logs `ZG335JS sceneQA=1 player=T objects=<size> calls=<drawCalls> fps=<fps>`
     after 1 second;
  6. logs `ZG335JS sceneQA-error <msg>` on exception.
- No fake `THREE` or fake scene object was injected. `view51.js:207` imports
  `* as THREE`; the module loaded from the pak fallback.

## Device evidence

MuMu `emulator-5554`, WebView `110.0.5481.154.1`.

```text
I ZG335JS : ZG335JS asset vendor/build/three.module.js=200
I ZG335JS : ZG335JS asset vendor/examples/jsm/loaders/GLTFLoader.js=200
I ZG335JS : ZG335JS sceneQA=1 player=T objects=0 calls=7 fps=53
```

`objects=0` is expected for this minimal state (players/mobs/npcs/drops empty).
`calls=7` confirms the render loop is executing. No `sceneQA=missing`, no
`sceneQA-error`, and no fake fallback were logged.

The legacy UI loaded:

```text
I ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html?sceneQA=1
```

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,818,056 | `c93c43a6a4d0f3e1162eb2792e3e6de13f3d2a6f561f65a30060d1fb274eb9af` |
| `app-release-unsigned.apk` | 54,773,570 | `e1ca92e073f63d83bc542ba73cf39b3095ec826853aa15fcebd6c7b94f41a54a` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source hash

`app/src/main/java/com/zg335/host/MainActivity.java`:
12,881 bytes, SHA-256
`dcf5fccb669877e7417ff35ed31fdfddea462dd3880f1e4ba37861b673f2c483`.

No protocol/record-layer, view51.js, client.js, old Java/assets changes; no push.
