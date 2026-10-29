# T48 / T47 Final Parser Re-sign QA

- Audit date: 2026-10-28
- Scope: `android-host-smoke/bridge` parser, ABI test and temporary adversarial harness
- Mode: read-only product source; independent NDK r27 clang build; MuMu `emulator-5554` execution; no product/test/Java/JS changes; no push.
- Frozen identity requested and verified:
  - source `src/zg_bridge.c`: `bc521fb6216fcfa45c91f736fe757e113d96c58cc17ad8c8f2809c8e98073247` (23,096 bytes)
  - `tests/test_frame.c`: `126bc54b1af4c78ad2bcac3a44ca21eb41fe7b7264ca1b6b79de893210bbec4c` (15,794 bytes)
  - `tests/test_abi.c`: `0e68581bee251af4cf16e287510940bff6b193faf7320eca5327e341c1ee5ff6`
  - `include/zg_bridge.h`: `0419eea0a3d051ed02174960be86226e3d1ae39c8becf82d0ef998e31474af00`

## 1. Verdict

**PASS for the T47/T48 parser contract on the frozen source/test identities.**

All required negative controls passed on the live MuMu x86_64 device. The earlier gzip-trailing and UTF-8 acceptance failures are closed; caps are now 262144 tokens / 65536 keys; OVERSIZE clears the RX ring, and BACKPRESSURE intentionally preserves the accepted partial frame and recovers with the remaining byte.

## 2. Source-level contract verification

- Caps: `ZG_JSON_MAX_TOKENS = 262144`, `ZG_JSON_MAX_KEYS = 65536` at `src/zg_bridge.c:12-13`, enforced by `json_add_token()`/`json_add_key()`.
- Gzip: after `inflate`, `Z_STREAM_END && stream.avail_in != 0` is forced to `Z_DATA_ERROR` at `src/zg_bridge.c:562-564`; decode errors clear RX via `process_rx`.
- UTF-8: `json_utf8()` at lines 185-243 rejects stray continuation bytes, overlong C0/C1, invalid E0/ED/F0/F4 ranges, surrogates and truncated sequences. `json_string()` lines 271-328 validates `\u` hex and surrogate pairing.
- Feed policy:
  - `length > rx_capacity`: `ring_clear` then `OVERSIZE` (`src/zg_bridge.c:691-694`).
  - `length > ring_free`: `BACKPRESSURE` without clearing the accepted partial frame (`696-698`), matching the frozen T47 contract.
- ABI: `include/zg_bridge.h` and `tests/test_abi.c` static assertions for the 24-byte envelope and 12-byte record offsets remain unchanged.

## 3. Independent execution

Built with NDK r27 clang 18.0.4 for `x86_64-linux-android24` and executed on `emulator-5554`:

```text
BRIDGE-ABI PASS
FRAME-PASS tokens=5 keys=2
ABI_EXIT=0 FRAME_EXIT=0
```

The current `tests/test_frame.c` covers half header/body, one-byte feeds, two frames in one feed, trailing gzip GARBAGE/0x00/second member, invalid UTF-8, exact token/key caps, gzip output overflow, OVERSIZE recovery, malformed/truncated gzip, control envelope, zero/0xffffffff lengths, and BACKPRESSURE partial-frame recovery.

## 4. Independent adversarial harness

A temporary C harness (outside product/tests) was compiled against the frozen source and run on the same emulator. Results:

```text
utf8_chinese_emoji status=0 frames=1        (valid Chinese/emoji)
utf8_surrogate_pair status=0 frames=1       (valid \uD83D\uDE00)
utf8_80 status=4 frames=0                   (reject)
utf8_overlong_c0af status=4 frames=0
utf8_ff status=4 frames=0
utf8_overlong_e08080 status=4 frames=0
utf8_surrogate_ed status=4 frames=0
utf8_truncated status=4 frames=0
json_lone_high status=4 frames=0
json_lone_low status=4 frames=0
json_bad_pair status=4 frames=0
json_bad_hex status=4 frames=0
gzip_trailing_garbage status=4 frames=0
gzip_trailing_zero status=4 frames=0
gzip_second_member status=4 frames=0
token_array_n=262142 status=0 frames=1
token_array_n=262143 status=0 frames=1
token_array_n=262144 status=4 frames=0
feed_oversize status=5
feed_oversize_recovery status=0 frames=1
feed_backpressure status=6
feed_backpressure_recovery status=0 frames=1
ADVERSARIAL_FAILURES=0
```

This confirms the requested trailing/UTF-8/boundary/reset behavior independently of the author’s test file.

## 5. Findings summary

| Item | Result |
|---|---|
| Frozen source/test hashes | PASS |
| 262144 token / 65536 key caps | PASS |
| Exact token 262143 OK / 262144 PROTOCOL | PASS |
| gzip trailing GARBAGE/0x00/second member | PASS: PROTOCOL, frames 0 |
| Invalid UTF-8 and invalid `\u` | PASS: PROTOCOL |
| Valid Chinese/emoji and surrogate pair | PASS: accepted |
| OVERSIZE clears ring and recovers | PASS |
| BACKPRESSURE preserves partial frame and recovers | PASS per frozen contract |
| ABI 24B/12B offsets | PASS: unchanged static assertions |
| OOB/crash | None observed in these tests |
| Runtime/JNI/Java/JS/signing | NOT CLAIMED |

## 6. Boundary

This is local C/NDK/emulator parser QA only. It does not connect a real server, parse game commands, accumulate world state, produce render packets, validate JNI/Java/JS integration, sign, or grant production runtime acceptance. Product source, tests, Java and JS were not modified; no push was performed.
