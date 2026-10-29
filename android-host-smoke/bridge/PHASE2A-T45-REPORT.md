# Phase 2A QA-FAIL Fix Report (T45)

Scope: fix the independent QA failures in the C frame parser. No Java/JS/assets,
JNI, wire ABI fields/offsets or Phase0 tests were changed for this fix.

## Fixes

1. **Gzip trailing bytes**
   - After `inflate`, require `Z_STREAM_END` and `stream.avail_in == 0`.
   - Trailing bytes or a concatenated second gzip member now force
     `ZG_BRIDGE_PROTOCOL_ERROR` and clear RX.
2. **Strict UTF-8 inside JSON strings**
   - Raw bytes are validated as UTF-8: rejects stray continuation bytes,
     overlong `C0/C1`, invalid 3/4-byte ranges, surrogate UTF-8 (`ED A0..BF`),
     `F5+`, and truncated sequences.
   - `\u` escapes are checked for hex digits and surrogate pairing; lone
     high/low surrogates and invalid escapes fail.
3. **Explicit JSON token/key caps**
   - `ZG_JSON_MAX_TOKENS = 65536`
   - `ZG_JSON_MAX_KEYS = 16384`
   - Exceeding either returns `ZG_BRIDGE_PROTOCOL_ERROR` during tokenization.
4. **Feed-level error recovery**
   - `OVERSIZE` from `zg_bridge_feed` clears the RX ring before returning.
   - `BACKPRESSURE` does not clear already accepted bytes; the caller can feed
     the remaining bytes of the partial frame. A test feeds `frame_len-1`,
     gets backpressure for an over-large next chunk, then feeds the final byte
     and reaches `frames=1`.
5. **Test hardening**
   - trailing gzip garbage: `GARBAGE`, single `0x00`, second gzip member
   - invalid UTF-8: `0x80`, overlong `C0 AF`, `0xFF`
   - one-byte feeds
   - two complete frames in one feed
   - token-cap large array
   - gzip output cap / bomb-like expansion
   - truncated gzip
   - post-error valid-frame recovery
   - backpressure preserves partial frame and recovers with the final byte
   - zero and `0xFFFFFFFF` length prefixes
   - existing control/oversize/malformed cases

## Device execution

Toolchain: NDK r27 clang 18.0.4, CMake 3.22.1, x86_64, android-24.

```text
BRIDGE-ABI PASS
ABI_EXIT=0
FRAME-PASS tokens=5 keys=2
FRAME_EXIT=0
```

Executed on MuMu `emulator-5554` through `mumu-cli adb --vmindex 0`.
No failure output; no crash.

## Hashes

Diff against pre-hardening commit `922e28e`:
`bridge/T45-DIFF.patch` (23,907 bytes,
`2f7686d065e4ac3f36ecb14565ee8dffdbff9d0c7db1c734f44892ca5d99e7ef`).

| File | Bytes | SHA-256 |
|---|---:|---|
| `src/zg_bridge.c` | 23095 | `d5dfed8bbdfb4e9745265a12cde1897845da7ae722d53c9ac88c31574aa8e5b1` |
| `src/zg_bridge_internal.h` | 501 | `ac310a1ab3264e10b864793ba830f663ac4ef890b0a7ffc0c715dc8e75ecaff1` |
| `tests/test_abi.c` | 6587 | `0e68581bee251af4cf16e287510940bff6b193faf7320eca5327e341c1ee5ff6` |
| `tests/test_frame.c` | 12410 | `dd5e4eee61780b710b74958c5f682e55005c703973544bfcbd44d67754a04c53` |
| `CMakeLists.txt` | 1248 | `8ca020e849d7442342ec34cc38346bb27bdaed31de39c9e1f5a0736f8d14c3ff` |
| `build-x86_64/zg_bridge_test_abi` | 47312 | `2afc22982e4bfc78957cad66c9729f23d5e97a7bd360a434b5f4e44255f2d033` |
| `build-x86_64/zg_bridge_test_frame` | 76384 | `32b87dd38817a7da5c3146966067c2be39d2f39770574c39cec769f54b7aac52` |
| `build-x86_64/libzg_bridge.a` | 55790 | `6e15353cdfd5c73c81dc16b8fcdb9fb0e569c42454686db23831c94c74a43c01` |
| `T45-DIFF.patch` | 23907 | `2f7686d065e4ac3f36ecb14565ee8dffdbff9d0c7db1c734f44892ca5d99e7ef` |

## Boundary

This closes the Phase2A parser negative-control test gaps. It does not add game
fields, state accumulation, render packets, JNI/JS integration or production
runtime acceptance. No push was performed.
