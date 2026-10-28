# Behavior probe result

Status: **LOCAL HARNESS AUTOMATED GATE PASS (x86_64 + arm64 via MuMu translation) — NOT RUNTIME ACCEPTANCE**

## Result

Independently rebuilt and rerun by the lead on `emulator-5554` (x86_64,
Android 12):

```text
behavior state=2 raw_state=12 reason=0 dispatched=8 continued=8 normal=8 overflow=0 eax=00020000
normal[0] state=13 reason=0
normal[1] state=13 reason=0
normal[2] state=13 reason=0
normal[3] state=13 reason=0
normal[4] state=13 reason=0
normal[5] state=13 reason=0
normal[6] state=13 reason=0
normal[7] state=13 reason=0
```

Mapping:

- `state=2` = `ZG335116_HOST_COMPLETED`
- `raw_state=12` = `ZG335_RETURNED`
- `normal[*].state=13` = `ZG116_RETURNED`
- `dispatched=8`, `continued=8`, `normal=8`, `overflow=0`

The harness runs both F4 setup branches: `empty` and `nonempty`. The lead
independently reran both modes on x86_64 and ARM64-under-MuMu-translation; both
printed `behavior mode=<mode> RESULT PASS` and exited 0.

## Nonreturn matrix

The harness also forces the first machine callback to return each of the three
nonreturn services. On both ABIs, all three cases pass:

| service | raw state | normal[0] | projected | counts |
|---|---|---|---|---|
| STOPPED (2) | 7 CALLEE_STOPPED | 8 CALLEE_STOPPED | 2 LOWER_STOPPED | 1 dispatched / 0 continued / 1 normal |
| INVALID (3) | 8 CALLEE_INVALID | 9 CALLEE_INVALID | 3 LOWER_INVALID | 1 / 0 / 1 |
| UNKNOWN (4) | 9 CALLEE_UNKNOWN | 10 CALLEE_UNKNOWN | 4 LOWER_UNKNOWN | 1 / 0 / 1 |

Each case exits 0 only when the host remains `COMPLETED`, raw reason is 0,
normal/projected reasons are 0, counts are 1/0/1, and overflow is 0.

## Memory stop

The harness also forces the first `NormalOuter` read of `N0(0) = 0x00060000`
to return STOPPED. Both ABIs pass:

```text
behavior memstop state=2 raw_state=7 dispatched=1 continued=0 normal=1 overflow=0
behavior memstop normal0=4 projected=2
behavior memstop RESULT PASS
```

This maps normal `MEMORY_STOPPED` (4), projected `LOWER_STOPPED` (2), and raw
`CALLEE_STOPPED` (7), with 1 dispatched / 0 continued / 1 normal.

## Stale continuation

The first machine seam overwrites `S-0x20 = 0x0000ffe0` with
`0x2468ace0`. Both ABIs pass:

```text
behavior stale state=2 raw_state=11 raw_reason=9 dispatched=1 continued=0 normal=1 overflow=0 final=0
behavior stale normal0=13 normal_reason=0 final=1 target=2468ace0 cleanup=4 projected=1 projected_reason=0 returned_to=2468ace0
behavior stale RESULT PASS
```

Mapping:

- normal `RETURNED` (13), final target `0x2468ace0`, cleanup 4
- projected `LOWER_RETURNED` (1), returned_to `0x2468ace0`
- raw `RETURN_CONTROL_INVALID` (11), reason `RETURN_TARGET` (9)
- raw final target absent; 1 dispatched / 0 continued / 1 normal

## Malformed width16 vector

The harness corrupts the first read of `B+0x48 = 0x00030048` to initialize only
8 of 16 bytes. Both ABIs pass:

```text
behavior malformed state=2 raw_state=6 raw_reason=5 dispatched=8 continued=8 normal=8 overflow=0 last_addr=00030048 last_width=16
behavior malformed RESULT PASS
```

- raw `MEMORY_REPLY_INVALID` (6), reason `READ_INITIALIZATION` (5)
- 8 normal records retained
- last memory event at `0x00030048`, width 16

This is the first local behavioral path pass for the x86_64 Android harness.

The harness is now an automated gate, not just a print-only probe. It returns
exit 0 only when all of these hold:

- host state `COMPLETED`
- raw state `RETURNED`
- raw `reason=0` and `eax=0x00020000`
- `dispatched=8`, `continued=8`, `normal=8`
- `overflow=0`
- every one of the eight normal records is `ZG116_RETURNED` with `reason=0`

Both ABI runs print `behavior RESULT PASS` and exit 0.

## ARM64 result

The same `behavior_probe.c` was rebuilt for `aarch64-linux-android24` against
`android-arm64/lib/libzg335_portable.a` and run on the same MuMu instance. Its
output is identical:

```text
behavior state=2 raw_state=12 reason=0 dispatched=8 continued=8 normal=8 overflow=0 eax=00020000
normal[0..7] state=13 reason=0
```

MuMu is an x86_64 host running the ARM64 binary through Android ARM
translation. This proves the arm64 library links and the same harness path
completes under that translation layer; it is not a physical ARM device run.

## Root cause of the earlier early stop

The first harness reused `allocated_count` as both the preloaded-node count and
the allocation cursor. After setup it was 18, so the first allocate read
`allocated[18] = 0`, hit the zero-node failure path, and stopped after one
normal with `raw_state=7` / `normal[0]=8`.

The fix is harness-only:

- separate `allocate_index` and `machine_count`
- allocate reads `allocated[allocate_index]`
- XMM ordinal uses `machine_count + 1`

No frozen source, design, adapter, or A4 receipt was changed.

## Artifacts

- `harness/behavior_probe.c`
  SHA-256 `4f91d2b185c23d8b1ec6cd6c10d7c9c4fe158bc82b632a6e88fdedcff6852015`
- binary `build-local-x86_64/zg335_behavior_probe`
  SHA-256 `4a3b84e6a048d5af9bc03f1c9f5659f2c6bc5046a520be9d30f0102970609f31`
- binary `out/behavior-probe-arm64-v8a`
  SHA-256 `515b2412734614a64fe24cb3aa15ccc104d474ca06228eb1f1ccb79ead499fad`
- diagnosis `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`

## Boundary

This is one local MuMu harness run for x86_64 and one ARM64 run under MuMu's
translation layer. It does not prove:

- physical ARM64 device behavior
- production runtime
- SDK/OS/native ABI acceptance
- Host/F4 full runtime
- iOS

It is stronger than the earlier `BAD_INTERFACE` link probe, but it is still a
harness result, not a whole-product acceptance.
