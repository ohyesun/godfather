# Android C Binary Bridge Specification

Status: proposal for review; no code, assets, Java or C source changed by this
document.

Inputs:

- `android-host-smoke/ANDROID-JS-DATA-PATH.md` (T36)
- `state/oldjava/sources/com/zhanguo/chaptertwo` decompiled Java
- `android-host-smoke/prebuilt/include/zg335/*.h`
- `android-host-smoke/ANDROID-WEBVIEW-BRIDGE.md`

## 1. Decision summary

The final Android data path is:

```text
Java: Activity / WebView / lifecycle / TLS socket read-write only
  -> raw TLS plaintext bytes
C: transport queue -> frame boundary -> RC5/DES -> zlib/gzip -> protocol state
  -> compact binary render packets
JNI: byte[] / direct ByteBuffer only
  -> Java WebView binary message (ArrayBuffer)
JS: DataView / typed arrays -> shadow world / Three.js update only
```

No JSON object, JSON string, `JSONObject`, `JSON.stringify(world)` or
`mergeFrame109` participates in the frame path.

The old Java classes are **protocol references only**. They must not remain on
the final compile path except for Activity/WebView/lifecycle and the TLS socket
adapter.

## 2. Old chain segment mapping

### 2.1 Java entry points

| Old Java location | Current behavior | Final disposition |
|---|---|---|
| `MainActivity.java:82` `addJavascriptInterface(new Bridge(), "Lab")` | Exposes `Lab` object | Keep only lifecycle/control methods; no JSON frame methods |
| `MainActivity.java:139-149` `openStream109` | Constructs `MobileStream109` | Replace with `TlsTransport.open`; raw byte reader |
| `MainActivity.java:152-155` `sendStream109` | Queues JSON strings | Replace with binary `send(byte[])`; C owns app framing |
| `MainActivity.java:157-163` `closeStream109` | Closes stream | Keep lifecycle close; no protocol |
| `MainActivity.java:166-183` `request95` | Dispatches to `TcpTransport96.request` / `TcpTransport97.request` | Replace with `TlsTransport` raw read/write; C owns protocol |
| `MainActivity.java:187-210` `request95` worker | Calls Java transport and puts JSON metrics | Remove JSON; pass raw bytes + counters to C |
| `MainActivity.java:217` `evaluateJavascript("window.nativeReply(...)", str)` | Pushes JSON response into WebView | Replace with binary WebMessage/ArrayBuffer; no per-frame JS string |
| `MainActivity.java:122` `evaluateJavascript("window.nativeStream109(...)", str)` | Pushes JSON stream frame | Replace with binary WebMessage; no `JSONObject` conversion |
| `MainActivity.java:30-31` worker/realtime pools | 4 worker + 2 realtime threads | Keep bounded IO pools; one reader thread per socket, one decode owner |

`MainActivity` should not contain `new JSONObject`, `JSONObject.quote`, frame
field access, command switch, state accumulation, or world projection.

### 2.2 `MobileStream109`

| Old location | Current behavior | Target C module |
|---|---|---|
| `MobileStream109.java:45-68` `send(String)` | JSON parse for coalescing, 16-entry string queue | `bridge_tx_queue` + binary input queue |
| `MobileStream109.java:100-184` reader | TLS socket, HTTP `CONNECT`, 4-byte length, `GZIPInputStream`, `JSONObject` | `bridge_transport` + `bridge_frame` + `bridge_session` |
| `MobileStream109.java:157-170` | `readInt`; 1..2 MiB; gzip; `new String(...UTF-8)`; `new JSONObject`; `emit(JSONObject.toString())` | **Delete from final path**; C frame boundary/zlib/parser |
| `MobileStream109.java:188-200` writer | UTF-8 JSON line + `\n` | Binary TX frames from C; Java writes raw bytes only |
| `MobileStream109.java:202-210` `error()` | JSON `{closed,error}` | C error record / Java lifecycle event; no JSON frame |

The `CONNECT /mobile109` handshake and HTTP header parsing are transport
concerns. If retained, they belong in the Java TLS adapter or a C transport
module; they must not create the full world JSON.

### 2.3 `TcpTransport96` / `TcpTransport97`

| Old location | Current behavior | Target |
|---|---|---|
| `TcpTransport96.java:184-234` `DirectConnection` | Raw `Socket` send/receive; `receive()` returns available bytes | Java TLS/TCP transport only; C consumes bytes |
| `TcpTransport96.java:566-631` `directRequest` | `/direct/open`, `/direct/send`, `/direct/receive`, `/direct/close` | C transport/session; Java opens/closes raw socket only |
| `TcpTransport96.java:610-612` | `packetHex` decode + `send` | Binary `nativeFeed`/`nativeNextTx`; no hex |
| `TcpTransport96.java:613-614` | `encodeHex(receive())` -> `dataHex` | Binary ByteBuffer/byte[]; no hex |
| `TcpTransport96.java:483-505` | `decodeHex`/`encodeHex` | Remove from frame path; diagnostics may use hex offline |
| `TcpTransport96.java:507-516` `frame` | 18-byte BIG_ENDIAN gateway envelope | C `bridge_transport` / `bridge_frame`; no Java envelope |
| `TcpTransport96.java:519-535` `decodeFrame` | BIG_ENDIAN header + `new JSONObject` | C frame decoder; no Java JSON |
| `TcpTransport96.java:633-641` `directEnvelope` | JSON string result | C binary result/error packet |
| `TcpTransport96.java:643-680` `request` | Gateway request + JSON | C session; Java raw bytes only |
| `TcpTransport97.java:218-228` `frame` | 18-byte BIG_ENDIAN request | Reference only; C frame layer |
| `TcpTransport97.java:230-246` `decodeFrame` | Header + `new JSONObject` | Reference only; C frame layer |
| `TcpTransport97.java:262-308` `request` | TLS gateway + JSON | Reference only; final C session |

`TcpTransport96/97` are useful for the wire envelope and TLS/profile
validation, but their JSON and hex conversions are explicitly **not** in the
final data path. The old `TcpTransport97` fallback is not a second protocol in
the new build.

### 2.4 Old JS path (from T36)

| Old JS location | Current behavior | Target |
|---|---|---|
| `client.js:252417` `nativeReply` | `JSON.parse(r.body)` per native request | No JSON; Java posts ArrayBuffer; C packet |
| `client.js:249507` `DirectJsonConnection` | Polls `/direct/receive`, reads `body.dataHex` | C `bridge_transport`; binary receive |
| `client.js:249587-249596` `consume` | `fromHex` + `Uint8Array` + duplicate `decode` | Binary `Uint8Array` only; one decode |
| `client.js:249715-249718` `DirectJsonCodec.push` | `FrameCodec.push` + `directFrameToJson` | Remove JSON diagnostic path from production |
| `client.js:245922-245930` `GameClient.onData` | Receives byte array | Receives C binary frame/record |
| `client.js:246470,246498` `codec.push`, `handleFrame` | JS decrypt/framing/command dispatch | C `bridge_frame` + `bridge_session` |
| `client.js:5137` `DirectAppSession.step` | Full state/world snapshot | C `bridge_world`/`bridge_render` |
| `client.js:5362-5364` `world()` in step | Full world object | C render packet; JS shadow world update |
| `client.js:7681-7854` `buildWorld` | Projects full `full109` object | C `bridge_render`; no `full109` JSON |
| `client.js:262517` `acceptWorld109` | `mergeFrame109` + UI update | JS apply packet + shadow world + `view.update` |
| `frame109.js:1-7` `mergeFrame109` | Full/delta merge in JS | C delta builder; JS only applies typed records |
| `view51.js:762` `update(s)` | Three.js scene update | Keep unchanged |
| `hud109.js:190` `update(w)` | HUD update | Keep unchanged |

The old `full109`/`mergeFrame109` contract is a reference for delta semantics,
not a required wire shape for the new bridge.

### 2.5 C headers: what is reusable

`prebuilt/include/zg335` contains the portable VM/guest composition API:

- `zg335_entry`, `zg335_memory`, `zg335_result`
- `zg335116_config`, `zg335116_output`, `zg335116_run`
- `zg116la_*`, `zg_node_*`, `zg_range*`, `zg_wrapper154_*`

These are **not** the network protocol API. They describe the portable
guest/raw/range composition and are useful only where the C core must preserve
that frozen VM behavior. The Android bridge should expose a separate
`zg_bridge_*` ABI and link the portable library only where required. Do not
rename or overload `zg335116_run` as a network parser.

## 3. Target C modules and API

Proposed public C header (conceptual, not yet added):

```c
/* zg_bridge.h — proposed ABI, freeze before implementation */
typedef struct zg_bridge zg_bridge;

typedef struct zg_bridge_config {
    uint32_t abi_version;       /* ZG_BRIDGE_ABI_VERSION */
    uint32_t max_frame_bytes;   /* inbound app frame limit */
    uint32_t max_packet_bytes;  /* rendered packet limit */
    uint32_t max_tx_bytes;      /* outbound app frame limit */
    uint32_t flags;             /* reserved; must be 0 until frozen */
} zg_bridge_config;

typedef enum zg_bridge_status {
    ZG_BRIDGE_OK = 0,
    ZG_BRIDGE_NEED_MORE = 1,       /* partial frame, no output */
    ZG_BRIDGE_AGAIN = 2,            /* no packet currently available */
    ZG_BRIDGE_RESYNC_REQUIRED = 3,  /* base mismatch; request full snapshot */
    ZG_BRIDGE_PROTOCOL_ERROR = 4,
    ZG_BRIDGE_OVERSIZE = 5,
    ZG_BRIDGE_BACKPRESSURE = 6,
    ZG_BRIDGE_AUTH_REQUIRED = 7,
    ZG_BRIDGE_CLOSED = 8
} zg_bridge_status;

zg_bridge *zg_bridge_create(const zg_bridge_config *config);
void zg_bridge_destroy(zg_bridge *bridge);

/* Raw TLS plaintext bytes from Java; single writer. Never JSON. */
zg_bridge_status zg_bridge_feed(
    zg_bridge *bridge,
    const uint8_t *bytes,
    size_t length,
    uint64_t now_ms);

/* Nonblocking, single reader (UI/main thread). Returns packet bytes or status. */
zg_bridge_status zg_bridge_poll_render(
    zg_bridge *bridge,
    uint8_t *dst,
    size_t capacity,
    size_t *out_length);

/* Nonblocking outbound app bytes to the Java TLS writer. */
zg_bridge_status zg_bridge_poll_tx(
    zg_bridge *bridge,
    uint8_t *dst,
    size_t capacity,
    size_t *out_length);

/* Input command from JS after binary validation; C owns app crypto/framing. */
zg_bridge_status zg_bridge_queue_input(
    zg_bridge *bridge,
    const uint8_t *bytes,
    size_t length,
    uint64_t now_ms);

/* Request a full render packet after RESYNC_REQUIRED. */
zg_bridge_status zg_bridge_request_full(zg_bridge *bridge);

typedef struct zg_bridge_stats {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t frames;
    uint64_t render_packets;
    uint64_t render_dropped;
    uint64_t decode_ns;
    uint32_t render_queue_depth;
    uint32_t tx_queue_depth;
    uint32_t last_error;
} zg_bridge_stats;

void zg_bridge_get_stats(const zg_bridge *bridge, zg_bridge_stats *out);
```

### 3.1 Internal module split

| C module | Responsibility | Never does |
|---|---|---|
| `bridge_transport` | bounded RX ring, TX queue, session epoch, byte counters | parse game commands, allocate per-entity JS objects |
| `bridge_frame` | 4-byte length/flags, 8-byte block decrypt, RC5/DES, zlib/gzip, frame body | touch WebView, produce JSON |
| `bridge_session` | login/select/step state machine, command dispatch, revisions | call Java/WebView, hold Java object references |
| `bridge_world` | persistent entity/state tables, map/player/actors/events, gold/inventory revisions | render 3D, own DOM/Three.js |
| `bridge_render` | encode full/delta packets, dirty tracking, backpressure policy | read JS heap or WebView state |

### 3.2 Error and backpressure contract

- `zg_bridge_feed` must never block on allocation; it returns
  `ZG_BRIDGE_BACKPRESSURE` or `ZG_BRIDGE_OVERSIZE`.
- RX ring is bounded; on overflow, Java stops reading the TLS socket (TCP
  backpressure), not silently discard application frames.
- Render queue is bounded. A full/keyframe packet is never dropped; a stale
  non-keyframe render packet may be dropped only with `render_dropped++` and a
  subsequent `RESYNC_REQUIRED`.
- TX queue is bounded; when full, return busy to JS and keep the current app
  session; do not close the socket solely for one rejected input.
- Protocol errors are classified with a stable C enum; only a resync or
  transport failure closes the session.
- `poll_render`/`poll_tx` are `AGAIN` when empty and never sleep on the UI
  thread.

## 4. Binary packet layout

### 4.1 Envelope (proposal; freeze before implementation)

Little-endian, no pointers, no JSON, 4-byte aligned:

```text
offset  size  field
0       2     magic = 0x5A47 ('ZG')
2       1     version = 1
3       1     kind: 1=FULL, 2=DELTA, 3=CONTROL, 4=ERROR
4       2     flags (bit0 keyframe, bit1 resync, rest reserved=0)
6       2     header_bytes = 24
8       4     frame_id (monotonic)
12      4     base_frame_id (0 for FULL; previous frame for DELTA)
16      4     payload_bytes (after 24-byte header)
20      2     record_count
22      2     reserved = 0
24      ...   records
```

Constraints:

- `header_bytes` must be 24 in v1; readers reject other values.
- `payload_bytes` must be within `max_packet_bytes`.
- All integers little-endian; booleans u8; floats IEEE-754; strings UTF-8 with
  explicit byte length; no native pointers or `size_t` in the wire form.
- Record lengths are self-describing; unknown record types are skipped by
  length, never guessed.
- `DELTA` with mismatched `base_frame_id` forces `RESYNC_REQUIRED`; the next
  packet must be `FULL` or `CONTROL`.
- A packet contains one frame; do not concatenate independent frames.

### 4.2 Record envelope

```text
offset  size  field
0       2     record_type
2       2     schema_version (0 until field registry frozen)
4       4     entity_id (0 for non-entity records)
8       4     payload_bytes
12      ...   payload
```

Record families are bridge-level only; exact game field offsets are not frozen
here:

| `record_type` | Meaning | Field freeze |
|---|---|---|
| 1 `MAP` | map identity and scene generation | pending PC contract |
| 2 `SELF` | local player and camera/actor identity | pending |
| 3 `ACTOR_UPSERT` | player/NPC/mob/drop insert or update | pending |
| 4 `ACTOR_REMOVE` | entity id removal | pending |
| 5 `EVENT` | combat/animation/audio event | pending |
| 6 `HUD` | HUD-only state (money, cooldowns, channel) | pending |
| 7 `CONTROL` | resync/ack/state revision | envelope only |
| 8 `ERROR` | stable C error code and severity | envelope only |

Do not assign game field IDs or byte offsets until the PC frame contract is
frozen. The T36 report deliberately avoids inventing `player.x`/`actor.hp`
offsets; this spec does the same. The record header is stable; payload schemas
are a separate freeze gate.

### 4.3 Full vs delta

- `FULL`: contains all records required to reconstruct the current scene and
  HUD baseline. Generated on connect, resync, map change, or explicit request.
- `DELTA`: contains only changed records since `base_frame_id`; entity removal
  is explicit (`ACTOR_REMOVE`), never inferred from absence.
- `EVENT` records are append-only for the frame; JS uses a monotonic event id
  and ignores duplicates/old ids.
- `HUD` records are independent of Three.js actor records; HUD may update at a
  lower rate but must share `frame_id`.
- Strings (names, labels) use a length-prefixed UTF-8 slice inside a record.
  No C `char *` is retained after encoding.

## 5. JNI, Java and WebView model

### 5.1 JNI signatures (proposed)

```java
final class ZgBridge {
    static native long nativeCreate(int abiVersion, int maxFrameBytes,
                                    int maxPacketBytes, int maxTxBytes, int flags);
    static native void nativeDestroy(long handle);

    // TLS plaintext -> C; called by the single reader thread.
    static native int nativeFeed(long handle, java.nio.ByteBuffer src,
                                 int length, long nowMs);

    // C -> Java; called on the WebView/main thread; nonblocking.
    static native int nativePollRender(long handle, java.nio.ByteBuffer dst,
                                       int capacity);
    static native int nativePollTx(long handle, java.nio.ByteBuffer dst,
                                   int capacity);

    // JS input already encoded as binary; C owns app crypto/framing.
    static native int nativeQueueInput(long handle, java.nio.ByteBuffer src,
                                       int length, long nowMs);

    static native void nativeRequestFull(long handle);
    static native int nativeGetStats(long handle, int[] out, int length);
}
```

JNI rules:

- Only `byte[]` and direct `ByteBuffer` cross JNI. No `String`, `JSONObject`,
  `JSONArray`, or per-field getters on the frame path.
- `nativeFeed` copies or pins the buffer only for the call; C must not retain a
  `JNIEnv*`, local ref, or Java object after return.
- Return `int` status is `zg_bridge_status`; buffers are caller-owned.
- `nativePollRender`/`nativePollTx` return `AGAIN` when empty. They never block.
- `nativeGetStats` is a diagnostic snapshot; it may be called at low frequency.

### 5.2 Java transport

Keep in Java:

- `Activity`, `WebView`, lifecycle (`onPause/onResume/onDestroy`).
- `SSLSocket` / `Socket` creation, certificate pinning, connect/read/write.
- TLS read thread and write thread; TLS plaintext bytes only.
- Bounded executor and close handling.

Remove from Java:

- `JSONObject`, `JSONArray`, `JSONObject.quote`, `GZIPInputStream` on game
  frames, frame command dispatch, world/state projection, protocol state.
- `evaluateJavascript` carrying frame JSON.
- `MobileStream109` JSON/GZIP logic; `TcpTransport96/97` JSON/hex logic.

TLS vs application crypto:

- TLS is terminated in Java; C receives TLS plaintext.
- The game's RC5/DES application encryption, zlib/gzip, frame length and
  command state are C responsibilities.
- Java must not inspect command bytes or use `JSONObject` on them.

### 5.3 WebView binary transport

Do **not** send frame payloads through `addJavascriptInterface` string methods;
that recreates JSON/base64 and blocks the bridge thread.

Use a binary-capable WebMessage path:

- `WebViewCompat.addWebMessageListener` / `WebMessageCompat` with ArrayBuffer
  support (requires AndroidX WebKit). Java posts a `byte[]` ArrayBuffer to the
  page's `ZgBridgePort`; JS receives `event.data` as `ArrayBuffer`.
- JS posts input ArrayBuffers back on the same port; Java reads
  `WebMessageCompat.getArrayBuffer()` and calls `nativeQueueInput`.
- If the WebView runtime lacks ArrayBuffer messages, use a local
  `WebViewAssetLoader` binary endpoint or a direct native render surface; do not
  fall back to per-frame JSON or base64 `evaluateJavascript`.

This is the one external API detail to verify before implementation:
`WebMessageCompat` ArrayBuffer support depends on the AndroidX WebKit version
and WebView provider. Verify on the target MuMu WebView; record the exact
version in the build manifest.

### 5.4 Thread model

```text
[Java TLS reader thread] --plaintext bytes--> nativeFeed(handle, ByteBuffer)
        (single producer)                         |
                                                   v
                                      [C bridge_transport.rx]
                                                   |
                                      [C bridge_frame.decode]
                                                   |
                                      [C bridge_session/world]
                                                   |
                                      [C bridge_render queue]
                                                   |
[Java WebView main thread] <--nativePollRender-- [C queue]
        |
        +--> WebMessage ArrayBuffer --> JS DataView/typed array
        +--> shadow world -> unchanged view51.update / hud109.update

[JS input ArrayBuffer] --> WebMessage --> Java main
        --> nativeQueueInput --> C session/tx queue
        --> nativePollTx --> [Java TLS writer thread] --> TLS socket
```

Rules:

- One reader per session; one C decode owner. Do not call `bridge_feed`
  concurrently from multiple sockets.
- C never calls Java/WebView. Java polls and posts on the UI thread.
- `nativeFeed` should do bounded work; long parsing may be moved to a native
  worker, but the session state remains single-owner.
- `nativePollRender` and `nativePollTx` are nonblocking; Java posts only when
  bytes are available, coalescing/batching packets if the queue is deep.
- `onPause` stops the render pump but not necessarily the socket; `onDestroy`
  closes socket + native handle, idempotently.
- Do not hold the WebView `JavascriptInterface` lock while calling native or
  posting binary messages.

## 6. Migration order and acceptance

### Phase 0 — ABI freeze and corpus

Deliverable: `zg_bridge.h`, envelope/record registry, status enum, size limits,
error catalog, and golden captures from the legacy client.

Acceptance:

- Header size/endianness/limits are frozen in a test.
- Every error code has a deterministic test.
- Captured legacy bytes include login, select, scene, step, malformed/stop
  cases, and unknown commands.
- No code path contains `JSONObject`/`JSON.stringify` on the frame path.

### Phase 1 — Java transport raw bytes only

Replace `MobileStream109`/`TcpTransport96.directRequest` frame paths with a
`TlsTransport` that only does connect/read/write; C `feed` handles the bytes.

Acceptance:

- Java reads/writes only `byte[]`; grep of final source has no frame
  `JSONObject`, `GZIPInputStream`, `encodeHex`, `decodeHex`.
- C `bridge_frame` output bodies are byte-identical to the old `FrameCodec`
  for the captured corpus.
- `bridge_stats.rx_bytes/frames` match capture; no full-frame JSON created.
- Unit tests cover partial reads, 8-byte block boundaries, zlib trailing zeros,
  unknown commands, and oversize frames.

### Phase 2 — C session/state parity

Move `GameClient.handleFrame`/`DirectAppSession` state accumulation into
`bridge_session` + `bridge_world`.

Acceptance:

- For the captured session, C emits the same ordered command/event IDs and the
  same final player/map/actor/state identities as the legacy JS path.
- Field-level parity is checked by a test adapter, not by visual inspection.
- C state is persistent across frames; no full-world rebuild per step.
- Malformed/stop/resync cases produce the same session outcome as the legacy
  path (where comparable), with stable error codes.

### Phase 3 — C render packets and JS adapter

Generate `FULL`/`DELTA` packets; JS applies them to a shadow world and calls the
existing `view51.update`/`hud109.update`. Do not change the renderer.

Acceptance:

- No `JSON.parse`/`JSON.stringify`/`mergeFrame109` on the frame path.
- Same scene at the same capture produces identical actor set, positions,
  visual IDs, HP/dead flags, and events.
- `RESYNC_REQUIRED` path produces a full packet and recovers.
- JS adapter handles unknown records by length and does not throw the UI.

### Phase 4 — Performance/rollout

Measure on the same MuMu x86_64 and ARM-translated profiles as T36:

- bytes/frame through Java/WebView bridge;
- JS decode time per packet;
- main-thread long tasks/jank;
- FPS while moving and in combat;
- peak JS heap;
- C decode time and queue depth.

Acceptance: no full-world JSON conversion on the frame path and no regression
in frame time against the legacy baseline. Keep a feature flag to switch back
to the legacy read-only path for one release; do not make the C bridge
mandatory before the parity gates pass.

## 7. Explicit non-goals

- Do not keep `MobileStream109` GZIP+JSON reader, `TcpTransport96/97` JSON/hex
  request paths, `JSONObject` frame parsing, or `evaluateJavascript` full-frame
  JSON in the final build.
- Do not parse game protocol in Java, Kotlin, or JS. Java owns TLS socket and
  lifecycle only.
- Do not generate world JSON in C or JS. No `full109` JSON, no `mergeFrame109`
  on the wire path.
- Do not put game state authority in JS; C state is authoritative. JS shadow
  world is a render cache only.
- Do not use `addJavascriptInterface` string methods for frame payloads.
- Do not use base64, hex, `JSONObject.toString`, or per-frame
  `evaluateJavascript` as a binary workaround.
- Do not invent game field offsets/IDs before the PC frame contract freeze.
  The envelope and record framing may freeze first; payload field schemas may
  not.
- Do not change `view51.js` rendering behavior, `hud109.js`, Three.js scene
  construction, UI panels, or assets. The adapter only supplies typed data and
  calls existing update functions.
- Do not make Java call C per field/entity; batch through one feed/poll packet.
- Do not drop a full/keyframe packet for backpressure; resync explicitly.
- Do not claim runtime/SDK/OS/device acceptance from the C bridge unit tests.
  Android TLS/WebView integration remains a separate gate.

## 8. Open freeze gates and risks

1. **Field registry**: `record_type` and envelope can freeze now; game field
   IDs/offsets require the PC frame contract. Until then, C may emit only
   opaque/control records and tests, not production actor fields.
2. **WebView binary API**: verify `WebMessageCompat` ArrayBuffer support on the
   target WebView; if unavailable, choose `WebViewAssetLoader` binary transport
   or native GL, never JSON fallback.
3. **Auth/TLS ownership**: Java owns TLS; C owns app crypto. The split must be
   tested with a pinned certificate and a real socket; do not duplicate TLS in
   C.
4. **Backpressure policy**: full snapshots cannot be dropped; define queue
   watermark and resync trigger with QA.
5. **Legacy behavior parity**: `frame109` delta semantics and unknown command
   handling must be reproduced from captures, not inferred from field names.
6. **Performance baseline**: T36 gives static size estimates only; capture
   actual bytes/frame and JS heap before setting numeric thresholds.

## 9. References

- T36: `android-host-smoke/ANDROID-JS-DATA-PATH.md`
- Old Java: `state/oldjava/sources/com/zhanguo/chaptertwo/MobileStream109.java`,
  `TcpTransport96.java`, `TcpTransport97.java`, `MainActivity.java`,
  `PackedAssets107.java`
- C API: `android-host-smoke/prebuilt/include/zg335/zg335_portable.h`,
  `zg_copy335_rich116.h`, `zg116la_common.h`
- Existing boundary note: `android-host-smoke/ANDROID-WEBVIEW-BRIDGE.md`

This document is specification-only. No client.js, assets, Java, C, or build
files were modified; no build, Host run, device run or push was performed.
