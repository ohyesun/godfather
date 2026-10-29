# JNI Render Poll Phase 3B1 Report

Task: T50. Scope: retrieve the C CONTROL render packet through JNI into a
direct ByteBuffer. No WebView/JS integration.

## Implementation

`jni_bridge.c`:

- `Java_com_zg335_host_ZgBridge_nativeRequestFull`
  - calls `zg_bridge_request_full`;
  - null handle returns `ZG_BRIDGE_PROTOCOL_ERROR`.
- `Java_com_zg335_host_ZgBridge_nativePollRender`
  - validates a direct ByteBuffer and capacity;
  - calls `zg_bridge_poll_render`;
  - returns the packet length on success, otherwise returns `AGAIN=2`,
    `OVERSIZE=5` or `PROTOCOL_ERROR=4` unchanged.
- Both symbols are exported in `libzg335host.so`.

`ZgBridge.java`:

- `requestFull()`
- `pollRender(ByteBuffer destination)`
- direct ByteBuffer only; no JSON/String frame path.

`MainActivity` debug:

- existing control and gzip frame feeds run first;
- after 400 ms, calls `requestFull()`;
- polls a direct 24-byte little-endian ByteBuffer;
- logs the envelope and a second poll.

## MuMu logcat

```text
I ZG335HOST: state=1 normal=0 overflow=0
I ZG335BRIDGE: status=0 rx=24 frames=1 error=0
I ZG335BRIDGE: frame status=0 rx=63 frames=2 error=0 before=24
I ZG335BRIDGE: render len=24 kind=3 frame=1 magic=5A47
I ZG335BRIDGE: render again=2
```

- first poll return: `24` (success, 24-byte CONTROL envelope)
- second poll return: `2` (`ZG_BRIDGE_AGAIN`)
- kind: `3` (`CONTROL`)
- magic: `5A47`
- frame id: `1`
- process pid `2991` remained alive.

## APK

- Path: `app/build/outputs/apk/debug/app-debug.apk`
- Size: 44,031,358 bytes
- SHA-256: `4adc6c8b0a3c05bea101a1b2824977999e189833cc54a2cd3e3ffac56b5a241c`

## JNI/Java hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `app/src/main/cpp/jni_bridge.c` | 4160 | `167eeda30815e7a38ece6ca32e4f2b3d1d1057301fed766751754b9e31d17534` |
| `app/src/main/java/com/zg335/host/ZgBridge.java` | 2114 | `ea3248f052c1bcec05515e667827bc19eba69666fb11e87a2a5cf7c02e6189d3` |
| `app/src/main/java/com/zg335/host/MainActivity.java` | 5715 | `3acc5345cd80ce2eb38c994b1a30f84b472ef8ded87dd52115d254bea239bc60` |

## Boundary

This stops at Java. It does not post the packet to WebView/JS, parse game
fields, change the bridge ABI, parser, assets, or old Java. No push was
performed.
