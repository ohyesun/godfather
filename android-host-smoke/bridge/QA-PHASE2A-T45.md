# T46 / T45 Parser Re-sign QA

- Audit date: 2026-10-28
- Scope: current `android-host-smoke/bridge/src/zg_bridge.c`, `src/zg_bridge_internal.h`, `tests/test_frame.c`, ABI test
- Mode: read-only product source; temporary test artifacts under `E:\zg-handoff-workbuddy\tmp-t44`; no product/test/Java/JS changes; no push.
- Execution: independent NDK r27 clang build and MuMu `emulator-5554` run of current ABI/FRAME tests.

## 1. Verdict

**FAIL for T46 re-sign against the requested T45 identity.**

The gzip trailing-garbage and UTF-8 fixes are present in the current code and the current FRAME/ABI tests pass, but the current files are not the requested `b07fa836.../d53505c...` pair, the token/key caps do not match the requested 262144/65536 values, and BACKPRESSURE still preserves rather than clears the RX ring.

## 2. Identity check

Requested:

- source `b07fa83633a607f20b5a802383f9c45456cab74ffe919953c953eb1e48938c98`
- `test_frame.c` `d53505c991fe4c56c793d8553387c2028c16c305ff7617c35960e56750f26173`

Actual current files:

- source `d5dfed8bbdfb4e9745265a12cde1897845da7ae722d53c9ac88c31574aa8e5b1` (23,095 bytes)
- `test_frame.c` `f9048422036dece27ad7cc867a433cf9abb5f31276f2896e68fa7b7de1e27c8e` (11,511 bytes)
- `zg_bridge_internal.h` unchanged `ac310a1ab3264e10b864793ba830f663ac4ef890b0a7ffc0c715dc8e75ecaff1`
- `include/zg_bridge.h` unchanged `0419eea0a3d051ed02174960be86226e3d1ae39c8becf82d0ef998e31474af00`

No `b07fa836...` source or `d53505c...` test file was found in the workspace. This is an evidence/freeze blocker: the current files cannot be represented as the requested T45 commit.

## 3. Gzip trailing bytes — PASS in current source

`src/zg_bridge.c:556-571` now checks:

```c
result = inflate(&stream, Z_FINISH);
plain_length = plain_capacity - stream.avail_out;
if (result == Z_STREAM_END && stream.avail_in != 0) {
    result = Z_DATA_ERROR; /* trailing bytes or concatenated gzip member */
}
```

The test file covers `GARBAGE`, a single `0x00`, and a second gzip member (`tests/test_frame.c:156-169` and the expanded current test). The current FRAME test output was:

```text
FRAME-PASS tokens=5 keys=2
FRAME_EXIT=0
```

So F1 (gzip trailing garbage) is fixed in the current actual source.

## 4. UTF-8 — PASS in current source

`json_utf8()` at `src/zg_bridge.c:185-243` validates:

- stray continuation bytes;
- overlong `C0/C1`;
- valid 2/3/4-byte ranges;
- `E0` overlong, `ED A0..BF` surrogate encodings, `F0`/`F4` range edges;
- truncated sequences.

`json_string()` at lines 271-328 validates `\u` hex, high/low surrogate pairing, and allowed escapes. Invalid cases `0x80`, `C0 AF`, `FF`, overlong, surrogate UTF-8, truncated sequence, lone high/low `\u`, invalid pairing and invalid hex are covered by the current `tests/test_frame.c:171-194` plus the current expanded test.

No invalid UTF-8 acceptance remains in the current source. Legal Chinese/emoji was not independently executed because the MuMu device disappeared after the ABI/FRAME run; static UTF-8 range checks support it.

## 5. Token/key caps — FAIL against requested values

Current constants:

- `ZG_JSON_MAX_TOKENS = 65536` (`src/zg_bridge.c:12`)
- `ZG_JSON_MAX_KEYS = 16384` (`src/zg_bridge.c:13`)

Requested:

- tokens `262144`
- keys `65536`

The current test uses a 300,000-number array and expects rejection, but that only proves the 65,536 cap. It does not prove the requested 262,144/65,536 boundaries. The current implementation would reject a legal 65,537-token frame that the requested cap would allow. This must be corrected or the requested caps must be explicitly renegotiated.

## 6. Feed OVERSIZE/BACKPRESSURE — CONCERN/FAIL against literal requirement

Current `zg_bridge_feed`:

- `length > rx_capacity`: `ring_clear` then `OVERSIZE` (current lines 691-694);
- `length > ring_free(...)`: returns `BACKPRESSURE` without `ring_clear` (current lines 696-698).

The current FRAME test intentionally expects BACKPRESSURE to preserve the accepted partial frame and recover by feeding the final byte (`tests/test_frame.c:142-154`). That is a valid recovery policy, but it is **not** “BACKPRESSURE ring_clear”. If the T46 acceptance literally requires ring_clear for both OVERSIZE and BACKPRESSURE, this remains a fail. If preserving a partial frame is the intended contract, the requirement and test must state that explicitly.

## 7. ABI and test execution

Independent NDK r27 clang build against the current source produced:

```text
BRIDGE-ABI PASS
FRAME-PASS tokens=5 keys=2
ABI_EXIT=0 FRAME_EXIT=0
```

This confirms the current ABI/FRAME tests pass on `emulator-5554`; `test_abi.c` static assertions for 24-byte envelope and 12-byte record offsets are unchanged. The source hash still does not match the requested T45 hash.

## 8. Remaining required actions

1. Freeze the actual source/test identities or restore the requested `b07/d535` pair; do not claim a T45 re-sign against `d5df/f904`.
2. Set the caps to 262144 tokens / 65536 keys, or obtain explicit approval for different values and update the test boundaries.
3. Decide BACKPRESSURE policy: ring_clear (as Task #46 wording says) or documented partial-frame preservation. Current behavior is preservation.
4. Re-run ABI/FRAME and the custom adversarial cases (gzip trailing, UTF-8 legal/illegal, token boundary, feed recovery) on a live MuMu device after the freeze.

## 9. Boundary

This report is source/static and current-device test evidence only. It grants no runtime, JNI, Java/JS, signing, game-command or whole-product acceptance. Product source, tests, Java and JS were not modified; no push was performed.
