# Old WebView Frame Data Path

Input APK: `D:/zg/apk114-bridge/output/apk114-minimal-r129b2-signed.apk`
Read-only analysis of extracted `assets/client.js` (19,632,086 bytes, 262,843 lines)
and `assets/frame109.js` (840 bytes). No APK/assets/client.js were modified.
No build, Host run or push.

## 0. Executive summary

The game path is not a browser WebSocket path. The native APK owns a raw TCP
socket and exposes it through `window.Lab.request95` / native callbacks. The
WebView receives either:

- a native JSON string that `JSON.parse` converts once per request
  (`client.js:252417`), or
- an already-materialized `message.frame` object on the native stream path
  (`client.js:262521`).

The main path is `/mobile/step` polling: `poll()` at `client.js:262539` sends
input JSON, the native response is parsed at `:252417`, `DirectAppSession.step`
returns a complete `world` object, and `acceptWorld109` applies it with
`mergeFrame109` before `view51.js:762 update(s)` and `hud109.js:190 update(w)`.

The expensive boundary is therefore **native serialization of the full world
JSON plus WebView `JSON.parse` every step**, not a `JSON.stringify(world)` in
JS. There is also a second inbound decode for the diagnostic `nativePackets`
ring (`DirectJsonConnection.decode`), which means the same received chunk is
decrypted/decompressed once for `GameClient` and once for the JSON diagnostic
projection.

## 1. WebSocket / HTTP receive entry points

There is **no `new WebSocket` and no `new Worker`** in `client.js` (count: 0).
The comment at `client.js:249381-249384` states the server is raw TCP and the
native path uses `jsb.Socket` / native-net; the browser preview uses a proxy.

### 1.1 Native TCP request/reply bridge

`window.Lab.request95` is called at `client.js:252419` for direct login and at
`:252420` for normal requests:

```js
// client.js:252419
window.ZgDirectGame114?.setNativeRequest((route,body)=>{
  if(!window.Lab?.request95)throw Error('直连登录必须在 APK 原生环境运行');
  ...
  window.Lab.request95(id, endpoint, route, '', '', JSON.stringify(body));
});
```

The native callback is `window.nativeReply` at `client.js:252417`:

```js
// client.js:252417 (excerpt)
const b=JSON.parse(r.body);
r.status===200?p.resolve(b):p.reject(...);
```

So the actual "network response → JS object" entry is:

```text
native raw socket
  → window.nativeReply(id, {body: JSON string, wireBytes, ...})   client.js:252417
  → JSON.parse(r.body)                                             client.js:252417
  → Promise resolve                                                client.js:252417
```

### 1.2 Direct socket receive poll

`DirectJsonConnection.poll` at `client.js:249572-249577` calls the native route
`/direct/receive`; `consume` at `:249587` reads the JSON response field
`body.dataHex`:

```js
// client.js:249587-249596
consume(result) {
  const body = resultBody(result);
  const value = body.dataHex;
  if (typeof value !== 'string' || !value) return;
  const bytes = fromHex(value);
  for (const listener of this.listeners)
    listener.onData?.(bytes.slice());
  void this.decode(bytes);
}
```

`fromHex` is at `client.js:249482`; it creates `new Uint8Array(value.length/2)`
and parses two hex chars per byte. This is a real binary→JS conversion point,
but preceded by a native **hex string inside JSON**.

### 1.3 `/step` polling entry

`poll()` is at `client.js:262539`:

```js
const incoming = await request('/step', pending);
...
acceptWorld109(incoming, networkMs);
```

`request` routes to `crossplay95` (`client.js:252423`), which uses
`window.Lab.request95` at `:252419/252420` in the APK, and falls back to
`fetch('/crossplay95'+route,...)` at `:252421` only for browser preview.
The `/step` interval is `setInterval(poll,80)` at `client.js:262546`.

### 1.4 Native stream path

`window.nativeStream109` is at `client.js:262521`; `message.frame` is already a
JS object (no JS `JSON.parse` on this path):

```js
// client.js:262521-262528
window.nativeStream109=(generation,message)=>{
  ...
  if(!message.frame)return;
  ...
  acceptWorld109(message.frame,networkMs);
}
```

`streamInput109` at `:262530` uses `window.Lab.openStream109` and
`window.Lab.sendStream109(JSON.stringify(input))` at `:262536`. This is the
streaming/duplex variant; the polling path above is still the controlling path.

### 1.5 Other HTTP fetches (not gameplay frames)

- `fetch('/crossplay95'+route, ...)` at `client.js:252421` — preview transport.
- `fetch(url)` at `:255557` — story asset.
- `fetch('server-profile95.json')` at `:262617` — profile selector.

None of these is the native gameplay frame path.

## 2. pako / zlib and binary views

There is **no pako** in the client (count: 0), no `DecompressionStream` library
import; decompression is runtime-native:

- `nodeZlib` optional `require('zlib')` at `client.js:248013-248020`.
- Native injection hook `setZlibImpl(impl)` at `:248024`.
- `DecompressionStream` / `CompressionStream` at `:248026-248032`.
- `zlibInflate` at `:248088-248094`.
- `inflateTolerant` at `:248101-248115`; it tolerates trailing zero padding and
  retries after trimming.
- Protocol flags: `FLAG_ENCRYPTED=0x80`, `FLAG_ZLIB=0x40` at `:248021-248022`.
- `FrameCodec.push` at `:248151`; zlib handling at `:248198-248203`.

Binary views are heavily used:

- `Uint8Array` appears 158 times; `DataView` 239 times in `client.js`.
- `DirectJsonConnection.fromHex` creates a `Uint8Array` at `:249482-249489`.
- `FrameCodec.push` merges TCP chunks, decrypts 8-byte blocks, and slices frames
  at `:248151-248223`.
- Typical binary parsers use `new DataView(b.buffer, b.byteOffset, b.byteLength)`
  (for example `client.js:1992`, `:223631-223637`, `:223863-223869`).

## 3. JSON.parse / JSON.stringify and per-frame conversion

`JSON.parse` occurs 14 times in `client.js`; the only hot gameplay response
parse is `client.js:252417` (`JSON.parse(r.body)`). The other parses are config,
offline storage, profile, or diagnostic paths (`:3371`, `:5470`, `:249498`,
`:249672`, `:253208`, `:253210`, `:256566`, `:257989`, `:261203`).

`JSON.stringify` occurs 154 times, but **no full world/frame stringify** is
used on the main render path:

- `client.js:252419/252420` stringifies the request body before passing it to
  native `Lab.request95`.
- `client.js:262536` stringifies stream input for `Lab.sendStream109`.
- `client.js:262546` stringifies the small diagnostics report every 2 seconds.
- Other occurrences are signatures/fingerprints/diagnostic rows, not frame data.

Per-step behavior:

```text
poll() [client.js:262539]
  → request('/step', pending) [client.js:252423]
  → native request95 + JSON.stringify(input) [client.js:252419/252420]
  → native returns JSON string
  → JSON.parse(r.body) [client.js:252417]   // once per step
  → DirectAppSession.step [client.js:5137]
  → world()/buildWorld [client.js:5362/7681]
  → acceptWorld109 [client.js:262517]
  → mergeFrame109 [frame109.js:1]
  → view.update [client.js:262517, view51.js:762]
```

Important: `DirectAppSession.step` explicitly says the response is a **full
snapshot**, not a delta:

```js
// client.js:5150-5152
// The 114 renderer accepts a complete frame. Native packets continue
// updating state asynchronously; this response is deliberately a
// full snapshot so a dropped poll cannot strand the WebView.
```

### 3.1 Double inbound decode

`DirectJsonConnection.consume` sends raw bytes to `GameClient` listeners and
also calls `this.decode(bytes)` (`client.js:249594-249596`). The diagnostic
decoder does another `FrameCodec.push` plus a JSON-safe packet projection:

```js
// client.js:249598-249601
async decode(bytes) {
  for (const packet of await this.decoder.push(bytes))
    this.onPacket?.(packet);
}
```

`DirectJsonCodec.push` (`:249715-249718`) calls `FrameCodec.push` and
`directFrameToJson` (`:249679-249699`). `onPacket` is wired at
`client.js:3333-3341` and retains the last 128 packets in `nativePackets`.
This is useful evidence, but it is duplicate work for every inbound chunk.

## 4. mergeFrame109 call point and incoming shape

Import: `client.js:252390`:

```js
import {mergeFrame109} from './frame109.js';
```

Actual call: `client.js:262517`, inside `acceptWorld109`:

```js
world=mergeFrame109(world,incoming);
...
view.update(world);
...
hud109.update(world);
```

`frame109.js:1-7` implements both full and delta forms:

```js
export function mergeFrame109(previous,incoming){
 if(!incoming.frame109||incoming.full109)return incoming;
 if(!previous||previous.frame109!==incoming.base109)throw Error('画面同步已重置，正在重连');
 const frame={...previous,...incoming.changes109,time:incoming.time,ack:incoming.ack,
   events:incoming.events,frame109:incoming.frame109};
 for(const [key,delta]of Object.entries(incoming.entities109??{})){
   const rows=new Map((previous[key]??[]).map(v=>[v.id,v]));
   for(const id of delta.remove)rows.delete(id);
   for(const value of delta.put)rows.set(value.id,value);
   for(const p of delta.patch110??[]){
     if(!rows.has(p.id))throw Error('画面同步已重置，正在重连');
     const value={...rows.get(p.id),...p.set};
     for(const name of p.unset??[])delete value[name];
     rows.set(p.id,value);
   }
   frame[key]=[...rows.values()];
 }
 return frame;
}
```

### 4.1 Incoming shape in this client

`DirectAppSession.step` returns `world()` at `client.js:5362-5364`; `world()`
delegates to `buildWorld()` at `:7681`. The full-frame return is
`client.js:7824-7854`:

```text
{
  full109: true, frame109: ++this.frame, generation: 1,
  time, ack, positionCorrection,
  events: this.eventsFeed.slice(-128),
  sceneToken,
  explorationPositions: [...<=256 points],
  explorationPosition,
  instanceAnswer, instanceAnswerRevision,
  noviceScene, combatRestriction,
  map: {id,name,instanceId,country},
  player, players, npcs, mobs, others,
  drops, entrances, channel, food, ladder,
  companions, mount, pet, mountStatus,
  buffs, effects, gather,
  meta: {bridge:'pc6020', generation, sceneToken,
         groundItems, systemPanels, navigation, actors},
  state: this.state()       // added by step(), client.js:5363
}
```

The delta form `base109/changes109/entities109` is supported by
`frame109.js`, but this client's `buildWorld` hard-codes `full109: true` at
`client.js:7824` and does not emit `base109/changes109/entities109`. Therefore
the normal path is always a **full snapshot**; the delta merge is effectively a
compatibility path for a different native stream that can supply deltas.

The `entities109` shape is:

```text
entities109: {
  [entityKey: string]: {
    remove: id[],
    put: object[],
    patch110?: [{id, set: object, unset?: string[]}]
  }
}
```

## 5. Frame size and field ownership

### 5.1 Size estimate

The client does not log `JSON.stringify(world).length`, so no exact production
byte count is available from this APK. Static bounds and the projection shape
give an order-of-magnitude estimate:

| Scene | Estimated full JSON |
|---|---|
| Login/empty or small map, no inventory | ~10–30 KB |
| Normal town/field with player + several NPCs/mobs and normal inventory | ~50–150 KB |
| Crowded scene, large inventory, many events/players | ~200–500 KB or more |

Why it can grow: `state()` (`client.js:7977-8047`) includes inventory,
equipment, skills, NPCs, other players, ground items, companions, quests,
mail/auction/social panels, character data and settings. `events` is capped at
128 (`client.js:7864-7865`) and exploration points at 256
(`client.js:1584`), but `npcs`, `others`, `players`, `drops` and `state`
collections are not globally capped by this projection. The native zlib layer
means wire bytes may be smaller than this; `report.transport107` tracks
`wireBytes/plainBytes` (`client.js:252417`) but the APK has no captured sample.

To obtain an exact number, instrument a local capture at `acceptWorld109` with
`JSON.stringify(incoming).length` and `JSON.stringify(incoming.state).length`.
That is deliberately not done here (read-only task).

### 5.2 Fields required by rendering

`view51.js:762-777 update(s)` consumes:

- `s.player`: id, uid, name, x, y, z, dir, level, visual/model, moveSpeed,
  hp/maxHp, mp/maxMp, dead, actionsBlocked, sprinting/weak/sitting/stalling,
  routeActive/routeTarget, mount/pet-adjacent fields.
- `s.map.id` and `s.time`: map selection and effects.
- `s.events`: `impact`, `cast-stop`, `cast-aim`, `damage`, `cast`, `dodge`,
  `jump`, `motion-result113`.
- `s.players`, `s.mobs`, `s.npcs`, `s.drops`: actor lists with id, x/z,
  visual/model, HP, name, quality, count, state flags.
- `s.mount`, `s.pet`, `s.companions`: companion rendering.
- `s.generation`, `s.frame109`, `s.ack`, `s.sceneToken`: frame/scene identity.
- `s.meta.bridge`, `s.meta.navigation`, `s.meta.actors`: PC bridge and actor
  policy flags.

`hud109.js:190 update(w)` additionally consumes HUD-only fields: `w.buffs`,
`w.channel`, `w.food`, `w.gather`, `w.player.gold/yuanbao`, `w.ladder`,
`w.events`, `w.world?` auxiliary data. These are UI, not Three.js scene data.

### 5.3 Mostly state/UI, not required by renderer

- `world.state` (inventory, equipment, skills, quests, chapters, mail, auction,
  guild, social, shop, settings, panels) — consumed by HTML panels and
  `hud109` via `state()`, not by `view51.update` for 3D scene construction.
- `instanceAnswer`, `noviceScene`, `combatRestriction`, `positionCorrection`.
- `meta.systemPanels`, `meta.groundItems` capability flags, `mountStatus`
  details, `channel`, `food`, `ladder`, `entrance` (some HUD flows only).
- `buffs`/`effects` are often empty in this projection; `events` drive combat
  effects instead.

### 5.4 Renderer boundary

`view51.js` is the UI/render layer; it must remain the consumer of a `world`
object with the shape above. A native bridge must not create Three.js meshes or
DOM nodes. It should produce data only. `hud109.js` and panel code must continue
to consume the same `world`/`state` shape.

## 6. Recommended C/JNI binary bridge insertion

### 6.1 Current actual chain

```text
native raw TCP socket
  → /direct/receive response JSON {dataHex}       client.js:249572-249589
  → fromHex → Uint8Array                          client.js:249482-249489
  → DirectJsonConnection.consume                  client.js:249587-249596
  → GameClient.onData (listener)                  client.js:245922-245930
  → FrameCodec.push (RC5/DES + zlib)              client.js:246470, 248151-248203
  → GameClient.handleFrame (binary command)       client.js:246489, 246498
  → DirectAppSession events/state                 client.js:5137, 7681-7854
  → native JSON response for /step                client.js:252417
  → JSON.parse(r.body)                            client.js:252417
  → mergeFrame109                                 client.js:262517 / frame109.js:1
  → view51.update(world)                          client.js:262517, view51.js:762
  → hud109.update(world)                          client.js:262517, hud109.js:190
```

### 6.2 Phase A: remove JSON/hex transport, keep JS domain parsing

Insert at the native boundary `window.Lab.request95` / `nativeReply`
(`client.js:252417`) and at `DirectJsonConnection.poll/consume`
(`client.js:249572-249589`):

- C/JNI owns the TCP socket, `/direct/receive`, and returns a `byte[]` or
  `ByteBuffer` (a JNI `jbyteArray` or direct `ByteBuffer`), not a hex string and
  not a JSON string.
- JS adapter `DirectJsonConnection` replaces `body.dataHex` + `fromHex` with the
  binary payload; `GameClient.onData` and `handleFrame` stay unchanged.
- Keep `FrameCodec.push` in JS for this phase to limit risk.

Benefit: removes per-chunk hex expansion and native JSON serialization/parse
for the transport envelope. It does not yet remove per-step world JSON if
`/step` still returns a serialized object.

### 6.3 Phase B: move frame crypto/framing/zlib to C

Move the equivalent of `FrameCodec.push` (`client.js:248151-248203`) into C:

- C owns TCP read buffering, 8-byte RC5/DES blocks, 4-byte length/flags,
  `FLAG_ZLIB` inflate, and frame boundary preservation.
- JNI returns a batch of binary frames `(flags, body)` or one `ByteBuffer` with
  a small length table.
- JS adapter at `GameClient.onData` (`client.js:245923-245930`) consumes those
  frames; `GameClient.handleFrame` (`:246498`) remains the parser initially.
- Keep `directFrameToJson`/`nativePackets` only as an opt-in diagnostic build;
  do not decode every chunk twice when diagnostics are off.

This is the highest-value low-risk split: C removes the hot binary/crypto/zlib
work, while JS keeps the existing protocol dispatch and world projection.

### 6.4 Phase C: C builds typed frame/world data, JS only adapts shape

For a full binary bridge, C can parse the command stream into a compact
`ZgFrame` (player, map, actors, events, state revision), and JNI returns a
direct `ByteBuffer`. The JS adapter at `acceptWorld109` (`client.js:262517`)
materializes the same `incoming` shape and calls the unchanged
`mergeFrame109`/`view.update`.

- C owns: TCP, RC5/DES, zlib, frame boundary, protocol command parsing, and
  compact entity/event packing.
- JNI owns: `nativeOpen`, `nativeSend`, `nativeReceive`, `nativeClose`;
  return `jbyteArray`/`ByteBuffer` and primitive counts, never JSON strings.
- JS adapter owns: bounded `ArrayBuffer` → typed arrays, entity/event mapping,
  fallback to existing JSON path, and `world` shape compatibility.
- `view51.js:762` and `hud109.js:190` remain untouched; no C/JNI code may
  allocate Three.js objects, DOM nodes, or UI state.

### 6.5 Insertion points summary

| Layer | Insert here | What changes |
|---|---|---|
| Native socket | `/direct/receive` response (currently `dataHex`) `client.js:249587` | JSON hex → `byte[]`/`ByteBuffer` |
| JNI | `window.Lab.request95`/`nativeReply` `client.js:252417-252420` | JSON string → typed binary callback |
| Frame codec | `FrameCodec.push` `client.js:248151` | RC5/DES/zlib/framing in C |
| Domain parse | `GameClient.handleFrame` `client.js:246498` | Optional C parser; otherwise unchanged |
| World adapter | `acceptWorld109` `client.js:262517` | Build same `incoming`/`world` shape from binary |
| Render | `view51.js:762`, `hud109.js:190` | **Do not change** |

The safest first milestone is Phase A+B: a binary `/direct/receive` and C
frame decode, with the existing `GameClient` and `world` projection. It removes
the per-chunk hex/JSON and duplicate diagnostic decode without touching UI
rendering. Phase C should be considered only after measured frame sizes and
field ownership are captured from a real session.

## 7. Verification boundary

This report is static source analysis only. It does not prove a running old
game session, exact frame bytes, memory/GC cost, or runtime correctness. The
line numbers refer to the extracted `assets/client.js` and `frame109.js` from
the specified APK. No APK/assets/UI file was modified, and no build, Host run,
device run or push was performed.
