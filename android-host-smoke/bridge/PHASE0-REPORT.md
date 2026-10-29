# C Bridge Phase 0 ABI Report

Task: T39. Scope: ABI freeze only; no Java, WebView, game command or game field
implementation.

## File tree

```text
android-host-smoke/bridge/
  include/zg_bridge.h
  src/zg_bridge.c
  tests/test_abi.c
  CMakeLists.txt
  build-x86_64/
    zg_bridge_test_abi
    libzg_bridge.a
```

## ABI frozen

- Status values: `ZG_BRIDGE_OK=0` through `ZG_BRIDGE_CLOSED=8`.
- Config: `abi_version`, `max_frame_bytes`, `max_packet_bytes`,
  `max_tx_bytes`, `flags`.
- Envelope: 24 bytes, little-endian, magic `0x5A47`, version `1`, kinds
  FULL/DELTA/CONTROL/ERROR, keyframe/resync flags.
- Envelope offsets: 0, 2, 3, 4, 6, 8, 12, 16, 20, 22, payload at 24.
- Record framing: 12-byte header (type/schema/entity_id/payload_bytes).
- Game field payloads and command parsing are deliberately not implemented.

## Implementation behavior

- `zg_bridge_create`: validates ABI version, reserved flags zero, non-zero
  limits, and limit <= 16 MiB; allocates bounded RX/TX rings.
- `zg_bridge_feed`: appends bounded bytes; returns `OVERSIZE` above the frame
  limit and `BACKPRESSURE` when the RX ring is full; no frame parsing.
- `zg_bridge_poll_render`: nonblocking; Phase 0 has no render producer, so an
  empty queue returns `AGAIN` with `out_length=0`.
- `zg_bridge_poll_tx`: nonblocking; empty returns `AGAIN`; undersized output
  returns `BACKPRESSURE`; one queued chunk is returned on success.
- `zg_bridge_queue_input`: bounded TX append; oversize/backpressure behavior.
- `zg_bridge_request_full`: records the request only.
- `zg_bridge_get_stats`: fills counters and live queue depth.

## Build and device test

Toolchain: Android NDK r27 `clang 18.0.4`, Android CMake 3.22.1, x86_64,
`android-24`, `-Wall -Wextra -Wpedantic -Werror`.

Device: existing `emulator-5554` (x86_64).

```text
BRIDGE-ABI PASS
EXIT=0
```

Test binary:

- ELF64, x86-64, PIE shared object
- NEEDED: `libm.so`, `libdl.so`, `libc.so`
- No JNI, Java, JSON or WebView dependency/source reference

Source inspection:

```text
grep -RniE 'json|jni|java|javascript|webview|evaluateJavascript' \
  bridge/include bridge/src bridge/tests bridge/CMakeLists.txt
(no matches)
```

## Hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `include/zg_bridge.h` | 3988 | `0419eea0a3d051ed02174960be86226e3d1ae39c8becf82d0ef998e31474af00` |
| `src/zg_bridge.c` | 6174 | `c823f0a867a4a919dd9570e53a49951d366876003809f36a82f889bb3b8aa8e3` |
| `tests/test_abi.c` | 6049 | `4a1cb9a0f027dd1373b716ee52429ce2405ea12e7aad9b4851dab3b92900403b` |
| `CMakeLists.txt` | 757 | `6e90d2c4405ec4ef6bc1dedf645e920787ec6afd35d3274da7e6236fc0468c2b` |
| `build-x86_64/zg_bridge_test_abi` | 26328 | `32f4c6c16e04814d226fcf5027c78edc86d79e4a3f2d2ed7cb6a478112eb6e30` |
| `build-x86_64/libzg_bridge.a` | 20720 | `693510feb5c887bcf65a0489d669ab2835fe650f34d19939cefd3341bd1a54a7` |

## Boundary

This is a local ABI/ring/gate test on an emulator. It does not implement the
game protocol, frame decryption/decompression, state machine, render packets,
JNI, WebView transport, or runtime acceptance. No client.js, assets, Java, app
build files, or existing C sources were modified; no push was performed.
