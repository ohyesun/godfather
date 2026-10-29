# C Frame Parser Phase 2A Independent QA

- Audit date: 2026-10-28
- Scope: `android-host-smoke/bridge/src/zg_bridge.c`, `src/zg_bridge_internal.h`, `tests/test_frame.c`, plus ABI source/test
- Reviewed source SHA: `67e94d4b46bc6f131a4d7a261de0980500b6ce0a0008147e6646c2ee17476b8f` (18,962 bytes), matching the task identity
- Internal header SHA: `ac310a1ab3264e10b864793ba830f663ac4ef890b0a7ffc0c715dc8e75ecaff1`
- `tests/test_frame.c` SHA: `524374da87efcec51da24cc4ef8c9fcc9d4382828ba774b2845042e6fe656ca0`
- `tests/test_abi.c` SHA: `0e68581bee251af4cf16e287510940bff6b193faf7320eca5327e341c1ee5ff6`
- Mode: source inspection plus read-only zlib/byte probes. No product source, tests, Java, JS or assets were modified. No push.
- Execution limitation: `adb devices` returned no attached device; the MuMu x86_64 device was not available for independent execution of the existing FRAME/ABI binaries. Findings below are source-traced and zlib-probe confirmed, not claimed as device execution.

## 1. Verdict

**FAIL due two negative-control violations: gzip trailing garbage is accepted as OK, and invalid UTF-8 inside a JSON string is accepted as OK.**

Additional CONCERNS: no explicit JSON token cap; feed-level OVERSIZE/BACKPRESSURE does not clear the RX ring; existing FRAME/ABI tests were not independently executed because no device was available.

No out-of-bounds or crash was found in the inspected paths; the failure is protocol acceptance of malformed inputs, not an observed memory-safety failure.

## 2. Critical findings

### F1 — gzip trailing garbage is silently accepted (FAIL)

Source: `src/zg_bridge.c:388-437`, particularly:

- `stream.next_in = (Bytef *)body; stream.avail_in = (uInt)length;`
- `result = inflate(&stream, Z_FINISH);`
- `plain_length = plain_capacity - stream.avail_out;`
- `if (result != Z_STREAM_END) { ... }`
- then `json_count(plain, plain_length, ...)` and `return ZG_BRIDGE_OK`.

There is no check that `stream.avail_in == 0` after `Z_STREAM_END`. zlib/gzip returns `Z_STREAM_END` after the first complete member and leaves trailing bytes unconsumed. The plain JSON is still valid, so the frame is accepted.

Input:

```text
body = gzip({"a":1}) || b"GARBAGE"
frame = BE32(len(body)) || body
```

Expected: `ZG_BRIDGE_PROTOCOL_ERROR`, no frame count increment, RX reset.

Actual (source trace): `inflate` returns `Z_STREAM_END`, `json_count` succeeds, `stats.frames` increments, status `ZG_BRIDGE_OK`. Read-only zlib probe confirms `unused_data` = 7 bytes with `eof=True` for the same body. A trailing `0x00` or second gzip header behaves the same.

This violates the task’s negative-control requirement that malformed gzip must return PROTOCOL/OVERSIZE and not be silently swallowed.

### F2 — invalid UTF-8 in a JSON string is accepted (FAIL)

Source: `src/zg_bridge.c:165-190` (`json_string`). The only byte validation is:

```c
} else if (value < 0x20) {
    return 0;
}
```

Bytes `0x80`, `0xC0`, `0xAF`, `0xFF`, etc. are accepted inside a quoted string. There is no UTF-8 validation and no surrogate/overlong-sequence rejection.

Input:

```text
body = gzip({"a":"\x80"})
```

Expected: `ZG_BRIDGE_PROTOCOL_ERROR` (invalid UTF-8).

Actual (source trace): `json_string` accepts the byte, `json_count` succeeds, `decode_gzip_json` returns `ZG_BRIDGE_OK`.

Read-only byte probe confirms the parser’s condition would not reject `0x80`, `0xC0 0xAF`, or `0xFF` because each is >= `0x20`. The same applies to `{"a":"\xC0\xAF"}` (overlong) and `{"a":"\xFF"}`.

## 3. Major concerns

### C1 — no explicit JSON token cap

Source: `src/zg_bridge.c:146-152` defines `JsonCursor.tokens/keys`; token increments occur in `json_string`, `json_literal`, `json_number`, `json_object`, and `json_array`. `ZG_JSON_MAX_DEPTH` is 64, but there is no token maximum and no rejection when the token count is large (lines 369-386). A 2 MiB array of `0,` tokens is within the frame limit and is accepted. It is bounded by input size; it is not a memory/object builder, but the requested explicit token limit is absent.

### C2 — feed-level OVERSIZE/BACKPRESSURE does not reset the RX ring

Source: `src/zg_bridge.c:528-551`. `process_rx` errors clear the ring at lines 458-467, 463-467, and 473-478 / 484-486, but the feed-level checks:

```c
if (length > bridge->rx_capacity ||
    length > ring_free(...)) {
    set_error(bridge, ...);
    return ...;
}
```

return `OVERSIZE`/`BACKPRESSURE` without `ring_clear`. If the ring already contains a partial header/frame, a subsequent valid frame is appended behind the stale partial bytes and cannot recover without another corrective feed. This is contrary to the requested “error then ring/state reset and later valid frame recovery” property unless feed-level rejection is intentionally excluded.

### C3 — no independent FRAME/ABI execution

`adb devices` returned no device; MuMu was not available. The existing `PHASE2A-REPORT.md` claims `BRIDGE-ABI PASS` and `FRAME-PASS`, but those are author-run results. The tests present in `tests/test_frame.c` cover half-header, half-body, control, oversize and deadbeef-gzip only; they do not cover trailing gzip, invalid UTF-8, 1-byte feeds, continuous multiple frames, or the feed-level reset case.

## 4. Checks that remain statically sound

- 4-byte BE length: `length == 0` returns `PROTOCOL_ERROR` and clears the ring (`zg_bridge.c:457-462`); `>2 MiB` or `>max_frame` returns `OVERSIZE` and clears (`463-468`); full frame is gated by `rx_count < length + 4` (`469-471`).
- The ring is bounded by `max_frame + 4`; `ring_append` is called only after `length <= ring_free`; `ring_copy`/`ring_drop` use modulo capacity. No simple OOB/integer-overflow path was found in the inspected frame path.
- A frame body is copied into a bounded `malloc(length)` and consumed; `length` is at most 2 MiB by the absolute limit.
- Normal/control frame loops use `for(;;)` but each successful path drops bytes; incomplete frames return `ZG_BRIDGE_OK` with no busy-loop.
- Gzip output is capped at `max_packet_bytes` capped to 2 MiB; a full output buffer with continued inflation is mapped to `OVERSIZE`, and truncated/non-gzip input returns `PROTOCOL_ERROR` by the `result != Z_STREAM_END` branch.
- ABI offsets remain frozen in `include/zg_bridge.h`; `tests/test_abi.c` contains static assertions for 24-byte envelope and 12-byte record offsets. Source-level ABI is unchanged. Runtime ABI execution is unverified (C3).
- No OOB or crash was observed or proven; the FAIL is semantic (malformed inputs accepted).

## 5. Required fixes before PASS

1. After `inflate`, require `result == Z_STREAM_END && stream.avail_in == 0` (and reject concatenated/trailing members) before JSON count; return `PROTOCOL_ERROR` and clear RX.
2. Validate UTF-8 strings, including overlong sequences, surrogate code points, and invalid continuation bytes; return `PROTOCOL_ERROR`.
3. Add and enforce an explicit JSON token/key maximum; reject with `PROTOCOL_ERROR` or `OVERSIZE` before counting unbounded input.
4. Decide and implement the feed-level error policy: clear the RX ring on OVERSIZE/BACKPRESSURE where recovery is required, or document that feed rejection is external to parser reset.
5. Add adversarial FRAME tests for trailing gzip, invalid UTF-8, 1-byte feeds, multiple frames, gzip bomb, empty/truncated gzip, 0/1/2 MiB/0xFFFFFFFF lengths, and post-error recovery; run on MuMu x86_64 and record actual output.

## 6. Verdict boundary

**FAIL for the Phase2A parser acceptance gate.** The library is non-empty and frame handling is largely bounded, but malformed gzip and invalid UTF-8 return `ZG_BRIDGE_OK`, directly violating the negative-control requirement. No iOS, Host, JNI, Java, JS, signing, runtime or whole-game acceptance is granted.
