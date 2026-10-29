# Phase 2A Parser Final Contract Freeze (T47)

Supersedes the interim T45 report values. Scope: parser caps, feed-error policy,
adversarial tests and identity freeze. No Phase0 ABI, Java, JS, assets or wire
fields were changed.

## Frozen caps

- `ZG_JSON_MAX_TOKENS = 262144`
- `ZG_JSON_MAX_KEYS = 65536`
- depth limit remains `64`.
- Exceeding either cap returns `ZG_BRIDGE_PROTOCOL_ERROR` during tokenization.

Boundary tests now prove:

- array with 262143 values (`262144` tokens) is accepted;
- array with 262144 values (`262145` tokens) is rejected;
- object with 65536 keys is accepted;
- object with 65537 keys is rejected.

## Frozen feed-error policy

- `OVERSIZE`: clear RX ring, return `ZG_BRIDGE_OVERSIZE`.
- `PROTOCOL_ERROR` from frame/gzip/JSON validation: clear RX ring.
- `BACKPRESSURE`: **preserve already accepted bytes**, return
  `ZG_BRIDGE_BACKPRESSURE`; a later feed may append the remaining bytes and
  complete the frame. This is the final contract.
- The earlier T46 wording that BACKPRESSURE also clears the ring is **void**.

The test feeds `frame_len-1`, receives BACKPRESSURE for an over-large next
chunk, then feeds the final byte and asserts `frames=1`.

## Negative controls and adversarial coverage

- trailing bytes after a gzip member: `GARBAGE`, single `0x00`, second gzip
  member all return `PROTOCOL_ERROR`; no frame count increment.
- invalid UTF-8 strings: `0x80`, overlong `C0 AF`, `0xFF`, isolated
  continuation, surrogate ranges and truncated sequences rejected.
- `\u` escapes: 4-hex validation and high/low surrogate pairing.
- gzip output cap / bomb-like expansion: `OVERSIZE`.
- truncated gzip: `PROTOCOL_ERROR`.
- one-byte-at-a-time feeds.
- two complete frames in one feed.
- post-error valid-frame recovery.
- zero and `0xFFFFFFFF` length prefixes.
- 24-byte T40 control envelope remains accepted.
- existing ABI offsets/status assertions unchanged.

## MuMu execution

MuMu `emulator-5554` was available through `mumu-cli adb --vmindex 0`.

```text
BRIDGE-ABI PASS
ABI_EXIT=0
FRAME-PASS tokens=5 keys=2
FRAME_EXIT=0
```

No failure output; no crash. The final `FRAME-PASS` line is emitted only after
all adversarial assertions above pass.

## Frozen identity

| File | Bytes | SHA-256 |
|---|---:|---|
| `src/zg_bridge.c` | 23096 | `bc521fb6216fcfa45c91f736fe757e113d96c58cc17ad8c8f2809c8e98073247` |
| `src/zg_bridge_internal.h` | 501 | `ac310a1ab3264e10b864793ba830f663ac4ef890b0a7ffc0c715dc8e75ecaff1` |
| `tests/test_abi.c` | 6587 | `0e68581bee251af4cf16e287510940bff6b193faf7320eca5327e341c1ee5ff6` |
| `tests/test_frame.c` | 15794 | `126bc54b1af4c78ad2bcac3a44ca21eb41fe7b7264ca1b6b79de893210bbec4c` |
| `CMakeLists.txt` | 1248 | `8ca020e849d7442342ec34cc38346bb27bdaed31de39c9e1f5a0736f8d14c3ff` |
| `build-x86_64/zg_bridge_test_abi` | 47312 | `9f24a58563ef5572b0cc9749dda7f3e5a9141ae4a5f3c539dd97b974cc7a809d` |
| `build-x86_64/zg_bridge_test_frame` | 85776 | `581ff7ec1c24c617c93f95eefb42ed1acf6a328045d366c181424b8809b38d6b` |
| `build-x86_64/libzg_bridge.a` | 56030 | `3ce9fc4937f4968a13b9bcd1c8546e08aef9ca4a9dac98e5a4591ed1b78992e1` |
| `T45-DIFF.patch` | 27395 | `0e4f60409298a18e68a7e8e1039a9bad54199f5113cdb8ef6e50ea93ddcbefbd` |

Diff base: commit `922e28e` (pre-hardening parser). The identity above is the
final source/test/bin set for this phase; no unrelated file is part of it.

## Boundary

This is a local parser/ABI contract freeze on MuMu x86_64. It does not grant
real-server, game-field, JNI/JS, whole-game or production runtime acceptance.
No push was performed.
