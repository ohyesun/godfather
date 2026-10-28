# Behavior probe result

Status: **LOCAL HARNESS PATH PASS (x86_64 MuMu) — NOT RUNTIME ACCEPTANCE**

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
  SHA-256 `bf90a27e4afd14270ddc26b036c3e364fa8c00c6a40770da74e6debcd663866f`
- binary `build-local-x86_64/zg335_behavior_probe`
  SHA-256 `e59f154e49bc94bc56d009dde38d80bd2af656966c5e0474125bf7e0b2e34a44`
- diagnosis `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`

## Boundary

This is one local x86_64 emulator harness run. It does not prove:

- ARM64 device behavior
- production runtime
- SDK/OS/native ABI acceptance
- Host/F4 full runtime
- iOS

It is stronger than the earlier `BAD_INTERFACE` link probe, but it is still a
harness result, not a whole-product acceptance.
