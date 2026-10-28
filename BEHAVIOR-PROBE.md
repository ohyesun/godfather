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

This is the first local behavioral path pass for the x86_64 Android harness.

The harness is now an automated gate, not just a print-only probe. It returns
exit 0 only when all of these hold:

- host state `COMPLETED`
- raw state `RETURNED`
- `dispatched=8`, `continued=8`, `normal=8`
- `overflow=0`
- every one of the eight normal records is `ZG116_RETURNED`

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
  SHA-256 `49ed3f412ae683cd86b6ff4c26752d6a7a25553c61f104cdae76076dbbd78562`
- binary `build-local-x86_64/zg335_behavior_probe`
  SHA-256 `06d95f351d4281758bffca89992f98024810a059427b750fc5046f33ba13958c`
- binary `out/behavior-probe-arm64-v8a`
  SHA-256 `500ab46e82da512190149b346c927e5ade0acba61e9f752ba5c3e8563273580d`
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
