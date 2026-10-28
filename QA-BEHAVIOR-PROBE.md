# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `adba7cdeddec1fdf4630878c01095916032cfc59768ed6353257087d8403ed0e` (16187 bytes), matching `BEHAVIOR-PROBE.md:108`
- Binary identities on disk match:
  - x86_64 `build-local-x86_64/zg335_behavior_probe` SHA-256 `54fae2d4f1f760d2d8d8b29c77e31023bd6197c013f6b2027b11213f2dd72364`
  - arm64-v8a `out/behavior-probe-arm64-v8a` SHA-256 `cda63f143038aa725a790f941586f91d6ed5a3f3028aa884e05ef906c7e9a0c9`
- Documentation identity: `BEHAVIOR-PROBE.md` SHA-256 `d1b46b556352cddeb1fb60ae0341373ef4b0b32dafd42c7f915b817684c0e35d`.

## 1. Verdict

**PASS for the current local automated behavior gates; not a runtime or platform acceptance.**

The memory-stop concern is resolved: the read trigger is armed only after an allocate seam and requires RAM space, address `0x60000`, width 4, and a not-yet-hit guard; the memory-stop case now asserts raw/normal/projected reasons as well as states and counts. The existing normal and forced-machine-nonreturn gates remain intact.

## 2. Memory-stop trigger

- `Probe` carries `stop_read_address`, `stop_read_width`, `stop_read_armed`, and `stop_read_hit` at `harness/behavior_probe.c:18-21`.
- `run_memory_stop_case()` uses empty setup and sets `stop_read_address=0x00060000`, `stop_read_width=4` at lines 321-324.
- `probe_machine()` arms the trigger only inside the `ZG116LA_SEAM_ALLOCATE` branch at lines 148-156; it is not armed for a generic/first read.
- `probe_read()` triggers only when `space == ZG335_SPACE_RAM`, armed, address matches, width matches, and not previously hit at lines 72-78; it returns STOPPED with reason `0x7002`.
- No production source or design file is substituted.

## 3. Memory-stop assertions

- `run_memory_stop_case()` is at `behavior_probe.c:309-369`; it calls actual `zg335116_run` at line 340.
- The gate checks host COMPLETED, raw `ZG335_CALLEE_STOPPED` (7), raw reason 0, dispatched 1, continued 0, normal 1, overflow 0, normal0 `ZG116_MEMORY_STOPPED` (4)/reason 0, and projected `ZG335_LOWER_STOPPED` (2)/reason 0 at lines 357-367.
- `RESULT PASS/FAIL` is printed after the checks at line 368; the function returns 0 only on success at line 369.
- `main()` ORs this result into `failed` at lines 450-451; no unconditional success.
- Constants match the design headers: raw 7, normal 4, projected 2. The expected counts are 1/0/1.

## 4. Normal and forced-machine gates retained

- Normal path: both empty/nonempty setup branches, actual `zg335116_run`, host/raw/reason/eax/counters/all eight normal state+reason checks at the current main block; result failures propagate.
- Forced machine nonreturn: STOPPED/INVALID/UNKNOWN cases still assert raw/normal/projected states, reasons, counts, and overflow before returning PASS.
- `allocate_index`, `allocated_count`, and `machine_count` remain independent; `probe_machine()` uses `machine_count` for the XMM ordinal and `allocate_index` for node selection.

Coverage note: the memory-stop case uses the empty setup only; the normal path covers both empty and nonempty setups. This is an explicit scope limitation, not a current failure.

## 5. Failure/echo and boundary

- No fabricated constants are used as output; all printed values come from `zg335116_run`/output structures.
- The only early return is genuine bind failure; wrong state/count/reason values produce non-zero exit.
- `BEHAVIOR-PROBE.md:3` labels the result a local automated-gate PASS, not runtime acceptance.
- The ARM64 result is explicitly MuMu x86_64 + Android ARM translation, not physical ARM.
- The README boundary excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.

## 6. Findings summary

| Item | Result |
|---|---|
| `stop_read_armed` and allocate-seam trigger | PASS |
| RAM/address/width/one-shot guard | PASS |
| raw 7 / normal 4 / projected 2 and counts 1/0/1 | PASS |
| raw/normal/projected reasons = 0 | PASS |
| unconditional success / fabricated output | PASS: none |
| normal and machine-nonreturn gates | PASS retained |
| stop-case setup coverage | Empty-only; documented coverage note |
| runtime/physical ARM/SDK/OS/F4 boundary | PASS: not claimed |

Overall: **PASS (local automated behavior gates), limited to the documented normal, forced-machine-nonreturn, and memory-stop paths.** No compile, Host, device, push or runtime acceptance was performed.
