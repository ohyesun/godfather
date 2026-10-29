# C Frame Boundary + Gzip + JSON Token Phase 2A Report

Task: T42. Scope: C RX frame boundary, 4-byte big-endian length, gzip inflate,
and JSON token/key counting. No game fields, no JNI/JS changes, no JSON output
to JS.

## Files

- `src/zg_bridge.c`
  - 4-byte big-endian length framing in the RX ring
  - 1..2 MiB absolute body limit; config `max_frame_bytes` limit
  - gzip inflate via `inflateInit2(&stream, 15 + 16)`
  - decompressed output capped at 2 MiB / `max_packet_bytes`
  - minimal JSON tokenizer counting tokens and object keys; no object/field
    construction
  - 24-byte T40 bridge-control envelope remains accepted for compatibility
  - errors clear the RX ring and return stable `PROTOCOL_ERROR`/`OVERSIZE`
- `src/zg_bridge_internal.h`
  - test-only token/key accessor; not part of public `zg_bridge_stats` or wire
    ABI
- `tests/test_frame.c`
  - builds gzip of `{"a":1,"b":"x"}` with zlib
  - prepends 4-byte BE length
  - feeds 2-byte header, 2-byte header remainder, half body, rest of body
  - checks `frames=1`, no error, tokens>0, keys=2
  - checks 24-byte control compatibility, oversize length, malformed gzip
- `tests/test_abi.c`
  - Phase0 field/offset assertions unchanged
  - feed section uses a 24-byte control envelope and a partial frame to keep
    the bounded-ring/backpressure test valid under framed RX
- `CMakeLists.txt`
  - links `z`
  - adds `zg_bridge_test_frame`

## Device results

Toolchain: NDK r27 clang 18.0.4, CMake 3.22.1, x86_64, android-24,
`-Wall -Wextra -Wpedantic -Werror`.

```text
BRIDGE-ABI PASS
FRAME-PASS tokens=5 keys=2
EXIT=0
```

Both ran on `emulator-5554`.

## Behavior details

- A frame is accepted only after the full `4 + length` bytes are present.
- The parser supports arbitrary feed splits, including half headers and half
  bodies.
- `stats.frames` increments only after a successful gzip+JSON frame (or the
  explicit 24-byte bridge-control envelope).
- `stats.rx_bytes` counts accepted transport bytes.
- `last_error` is `PROTOCOL_ERROR` for malformed gzip/JSON, `OVERSIZE` for
  length > config/2 MiB, and `BACKPRESSURE` for a bounded-ring overflow.
- No game command dispatch, entity fields, JNI or JS is touched.

## Hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `include/zg_bridge.h` | 3988 | `0419eea0a3d051ed02174960be86226e3d1ae39c8becf82d0ef998e31474af00` |
| `src/zg_bridge_internal.h` | 501 | `ac310a1ab3264e10b864793ba830f663ac4ef890b0a7ffc0c715dc8e75ecaff1` |
| `src/zg_bridge.c` | 18962 | `67e94d4b46bc6f131a4d7a261de0980500b6ce0a0008147e6646c2ee17476b8f` |
| `tests/test_abi.c` | 6587 | `0e68581bee251af4cf16e287510940bff6b193faf7320eca5327e341c1ee5ff6` |
| `tests/test_frame.c` | 4874 | `524374da87efcec51da24cc4ef8c9fcc9d4382828ba774b2845042e6fe656ca0` |
| `CMakeLists.txt` | 1248 | `8ca020e849d7442342ec34cc38346bb27bdaed31de39c9e1f5a0736f8d14c3ff` |
| `build-x86_64/zg_bridge_test_abi` | 42128 | `39330b2712b39fe73807da1a1a54ad880c709fef0bd99a2c2606f25e56f0e1e3` |
| `build-x86_64/zg_bridge_test_frame` | 41848 | `0e50213c1e70473fe8a1314868848afa095df996f20f9527952d980cb567cfc9` |
| `build-x86_64/libzg_bridge.a` | 46078 | `60ea67ac5748bc7eba0dfc51f6f947f975afa6766da299dddc07aafb3cc331fc` |

## Boundary

This is local C/NDK/emulator testing. It does not connect a real server, parse
game commands, accumulate world state, produce render packets, or touch
Java/JS/assets. No push was performed.
