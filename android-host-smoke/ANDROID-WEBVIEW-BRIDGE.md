# Android WebView bridge decision

## What we keep

The legacy WebView UI is reused: `index.html`, `client.js`, Three.js renderer,
HUD and assets. It already renders the game scene in the new APK.

## What we do not keep

The legacy data path is not reused. Its core defect is:

```text
network bytes -> JS object -> JSON -> WebView parse/merge
```

Every downstream update is serialized into JSON and pushed through the WebView
bridge, so the UI thread and JS heap pay for full-state conversion and parsing.
That is the blocking bottleneck, not the HTML/Three.js rendering itself.

## New boundary

```text
network bytes
  -> C core (libzg335_portable): framing, validation, state, frame deltas
  -> JNI: compact binary packet (byte[] / direct ByteBuffer)
  -> JS adapter: DataView / typed arrays
  -> Three.js scene update
```

Rules:

1. C core owns protocol parsing and persistent state.
2. The bridge carries binary packets with explicit little-endian layout; no
   `JSON.stringify` of the full world state.
3. JNI exposes a bounded packet buffer, not per-field getters.
4. JS applies deltas to the existing scene; it does not rebuild the world from
   a full JSON snapshot each frame.
5. WebView is UI/render only; it is not the protocol owner.
6. If typed-array transfer still shows jank, the fallback is native OpenGL for
   the world layer while keeping WebView for menus/HUD.

## Packet sketch

```text
u16 magic
u8  version
u8  kind          // frame_delta, entity_upsert, entity_remove, hud, event
u32 frame_id
u32 payload_len
payload[]         // fixed-layout records, entity ids and typed fields
```

Exact field layout is defined only after the PC-side frame contract is frozen;
do not invent a second protocol in JS.

## Performance gates

Measure before/after on the same MuMu device and same scene:

- bytes per frame through the bridge
- JS decode time per frame
- main-thread long tasks / jank
- FPS while moving and in combat
- peak JS heap

Acceptance for the new bridge: no full-world JSON conversion on the frame
path, and no regression in frame time against the legacy baseline.
