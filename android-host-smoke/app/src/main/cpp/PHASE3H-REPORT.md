# First Actor sceneQA Phase 3H Report

Task: T65. Adds mob 1001 to the T63 sceneQA state and verifies that
`GameView` creates a real object. No view51/client.js/protocol changes.

## Implementation

Injected state now includes:

```js
mobs: [{
  id: 1001, name: 'A', x: 3, z: 4, y: 0,
  hp: 10, maxHp: 10, hpKnown: true, dead: false,
  alive: true, kind: 2
}]
```

After `sceneQA.update(s)`, the adapter waits 1 second and logs:

- `q.objects.size`
- `mob1001` is true only when `q.objects.get('mob1001')` exists **and** its
  mesh is in `q.scene.children`
- `renderReport109.drawCalls`
- `renderReport109.fps`

## Device result

MuMu `emulator-5554`, WebView `110.0.5481.154.1`:

```text
I ZG335JS : ZG335JS sceneQA=1 player=T objects=1 mob1001=true calls=3 fps=4
```

- `objects=1`: the mob object was created.
- `mob1001=true`: the torus fallback mesh is in the GameView scene and its
  object-map entry is `mob1001`.
- No `sceneQA=missing`, `sceneQA-error`, or `page-error`.
- **`calls=3`, not `>7`.** This is the actual draw-call count for this minimal
  state: one torus fallback mob + avatar/fallback + shadow; no scene art or
  populated NPC/player/event lists are supplied. The `calls>7` target in the
  task text was not met and was not faked.

Legacy UI still loaded. Three.js/GLTFLoader remain 200 from the pak fallback.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,818,163 | `62545dd8c85b73554858cc71250de501e84d11f9752f0bf4c7b0ab1546fcec0c` |
| `app-release-unsigned.apk` | 54,773,570 | `4f328af7e3a844c682f0df51031f5e7f1a46485c55543b8bf99196780758cb2e` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source hash

`app/src/main/java/com/zg335/host/MainActivity.java`:
13,090 bytes, SHA-256
`2bfb2c5daa00247cea34b1abae53134b1c63f249fc6a0e56af0c409309eba068`.

No view51/client.js/protocol/record-layer changes; no push.
