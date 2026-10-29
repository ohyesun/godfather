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
   - `ZG_JSON_MAX_TOKENS = 262144`
   - `ZG_JSON_MAX_KEYS = 65536`
   - Exceeding either returns `ZG_BRIDGE_PROTOCOL_ERROR` during tokenization.
4. **Feed-level error recovery**
   - `OVERSIZE` and `BACKPRESSURE` from `zg_bridge_feed` now clear the RX ring
     before returning.
   - A later valid frame can be appended and parsed.
5. **Test hardening**
   - trailing gzip garbage
   - invalid UTF-8: `0x80`, overlong `C0 AF`, `0xFF`
   - one-byte feeds
   - two complete frames in one feed
   - token-cap large array
   - gzip output cap / bomb-like expansion
   - truncated gzip
   - post-error valid-frame recovery
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

| File | Bytes | SHA-256 |
|---|---:|---|
| `src/zg_bridge.c` | 23154 | `b07fa83633a607f20b5a802383f9c45456cab74ffe919953c953eb1e48938c98` |
| `src/zg_bridge_internal.h` | 501 | `ac310a1ab3264e10b864793ba830f663ac4ef890b0a7ffc0c715dc8e75ecaff1` |
| `tests/test_abi.c` | 6587 | `0e68581bee251af4cf16e287510940bff6b193faf7320eca5327e341c1ee5ff6` |
| `tests/test_frame.c` | 10900 | `d53505c991fe4c56c793d8553387c2028c16c305ff7617c35960e56750f26173` |
| `CMakeLists.txt` | 1248 | `8ca020e849d7442342ec34cc38346bb27bdaed31de39c9e1f5a0736f8d14c3ff` |
| `build-x86_64/zg_bridge_test_abi` | 47096 | `296d2c2149544267fc01bd8edecbdfe698aae60c5792d746619beee6f420fccf` |
| `build-x86_64/zg_bridge_test_frame` | 69744 | `fc9548cf7ee341a5fb6be82038c57a0eb2b002195c9b2141c9b8f37f18380b74` |
| `build-x86_64/libzg_bridge.a` | 55790 | `6e15353cdfd5c73c81dc16b8fcdb9fb0e569c42454686db23831c94c74a43c01` |

## Boundary

This closes the Phase2A parser negative-control test gaps. It does not add game
fields, state accumulation, render packets, JNI/JS integration or production
runtime acceptance. No push was performed.
