# Behavior Probe Early-Stop Diagnosis

Task: T24. Scope: local harness only; frozen sources/design unchanged.

## Symptom

Original MuMu x86_64 output:

```text
state=2 COMPLETED, raw_state=7 CALLEE_STOPPED
dispatched=1, normal=1
normal[0]=8 CALLEE_INVALID, overflow=0
```

## Root cause

`behavior_probe.c` used the same `allocated_count` field for two unrelated
purposes:

- setup() pre-populated 18 allocation nodes and incremented `allocated_count`
  to 18;
- probe_machine() used `allocated_count` as the *next allocation index*.

Therefore the first allocate call read `allocated[18]`, which is zero. The
raw52 node-provider followed its zero-node/failure path, yielding
`CALLEE_INVALID` and stopping the observed normal sequence.

The F4 fixture keeps these concepts separate:

- `Oracle::allocate_nodes` is the seeded node table;
- `Oracle::allocate_index` is the allocation cursor, initially zero;
- `oracle_machine()` uses the independent machine ordinal, not the allocation
  cursor, for XMM state.

## Minimal harness fix

In `harness/behavior_probe.c`:

1. Added `allocate_index` and `machine_count` fields to `Probe`.
2. `probe_machine()` now reads `allocated[allocate_index]` and increments
   `allocate_index`; it no longer uses `allocated_count` as the cursor.
3. `probe_machine()` uses `machine_count` for the machine/XMM ordinal.
4. `changed_xmm()` now applies F4's `ordinal + 1` multiplier.
5. `setup()` now takes `nonempty` and reproduces both F4 branches:
   `Q->Q`, N0-only for empty; `Q->C`, N0/N1, C fields for nonempty.
6. `main()` runs both modes, asserts host/raw/dispatched/continued/normal/
   overflow and all eight normal states, prints `RESULT PASS/FAIL`, and returns
   non-zero on any failure.

No production C, header, design or frozen source was changed.

## Local rebuild/run

Build directory: `build-local-x86_64` (Android NDK r27, x86_64).
Run target: existing `emulator-5554` (x86_64, Android 12).

Output after the fix:

```text
behavior mode=empty state=2 raw_state=12 reason=0 dispatched=8 continued=8 normal=8 overflow=0 eax=00020000
normal[0..7] state=13 reason=0
behavior mode=empty RESULT PASS
behavior mode=nonempty state=2 raw_state=12 reason=0 dispatched=8 continued=8 normal=8 overflow=0 eax=00020000
normal[0..7] state=13 reason=0
behavior mode=nonempty RESULT PASS
```

State mapping:

- host `2` = COMPLETED;
- raw `12` = RETURNED;
- normal `13` = RETURNED;
- `overflow=0`.

## Conclusion boundary

**Probe result: PASS on this local emulator.** Both `empty` and `nonempty`
setups reach the normal success path with eight returned normal records, print
`RESULT PASS`, and exit zero, rather than stopping at CALLEE_INVALID.

**Not a runtime acceptance claim.** This is one local x86_64 emulator run of a
harness. It does not establish production runtime, ARM64, SDK/OS, signing,
device, Host, or F4 full-runtime acceptance.
