# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `4360e97fd7656784fe871aa5ec10fca08f3694e98631844297259d27a073d7bd` (15487 bytes), matching `BEHAVIOR-PROBE.md:108`
- Binary identities on disk match:
  - x86_64 `build-local-x86_64/zg335_behavior_probe` SHA-256 `07df94abce3fc6c325a474523e5d5a426942c83b17c7931db018fad0b34c1818`
  - arm64-v8a `out/behavior-probe-arm64-v8a` SHA-256 `1d3f1070b1de10f95958d09ffacf4912b272f9306ab7e5022755b7737f4d7718`
- Documentation identity: `BEHAVIOR-PROBE.md` SHA-256 `9da83415e61955daffa787e2c4df76c41a9ff5ff1bf0cd51d1343c87b0d3d6cc`.

## 1. Verdict

**PASS for the current local automated harness gates; one non-blocking CONCERN about the memory-stop assertion completeness.**

The harness retains the 8-normal empty/nonempty gates and the three forced machine nonreturn gates, and adds a memory-stop case. The reported MuMu x86_64 and ARM64-via-translation outputs are consistent with the source and documented matrices. This is not runtime, physical-ARM, SDK/OS or F4 full-runtime acceptance.

## 2. Normal-path gate retained

PASS:

- `main()` loops `nonempty=0/1` at `behavior_probe.c:364-375`.
- The normal gate checks host COMPLETED, raw RETURNED, raw reason 0, EAX `0x00020000`, dispatched/continued/normal 8, overflow 0, and all eight normal states/reasons at current lines 409-423.
- `RESULT PASS/FAIL` is printed after the checks and failure propagates through `failed`.
- `allocate_index`, `allocated_count`, and `machine_count` remain independent at lines 13-19 and probe lines 123-139.

## 3. Forced machine nonreturn matrix

PASS for the three existing nonreturn cases:

`run_stop_case()` at `behavior_probe.c:241-301` seeds setup, sets `stop_state`, runs actual `zg335116_run`, and checks raw/normal/projected states, reasons, counts and overflow at lines 288-297. `main()` invokes STOPPED/INVALID/UNKNOWN at lines 430-435. The mappings remain:

| service | raw | normal0 | projected | counts |
|---|---|---|---|---|
| STOPPED 2 | 7 | 8 | 2 | 1/0/1 |
| INVALID 3 | 8 | 9 | 3 | 1/0/1 |
| UNKNOWN 4 | 9 | 10 | 4 | 1/0/1 |

These use actual callback returns and fail non-zero; no canned output.

## 4. Memory-stop case

PASS for the requested 0x60000 path, with a CONCERN:

- `Probe` carries `stop_read_address` and `stop_read_hit` at `behavior_probe.c:17-19`.
- `run_memory_stop_case()` clears the probe, uses `setup(...,0)`, sets `probe.stop_read_address = 0x00060000U` at lines 303-317, binds the real configuration, calls actual `zg335116_run` at line 333, and returns `ok ? 0 : 1` at lines 354-355.
- `probe_read()` checks the first matching read at lines 70-75 and returns `ZG335_SERVICE_STOPPED` with reason `0x7002`, exactly once via `stop_read_hit`.
- The expected constants match:
  - `ZG335_CALLEE_STOPPED = 7` (`raw`)
  - `ZG116_MEMORY_STOPPED = 4` (`normal0`)
  - `ZG335_LOWER_STOPPED = 2` (`projected`)
  - counts `1 dispatched / 0 continued / 1 normal`, overflow 0.
- The `ok` predicate at lines 346-353 checks host COMPLETED, raw 7, dispatched 1, continued 0, normal 1, overflow 0, normal0 MEMORY_STOPPED, and projected LOWER_STOPPED.
- `main()` ORs the result into `failed` at line 436 and returns it at line 437; there is no unconditional success.
- No out-of-bounds access: `normal_records[0]` is within the fixed record array and the predicate requires `normal_count32 == 1` before using it.

CONCERN: `run_stop_case` checks raw/normal/projected reasons, but `run_memory_stop_case` does not assert `output.raw_result.reason32`, `normal_records[0].normal_result.reason32`, or `projected_reply.reason32`. It also triggers solely by address `0x60000` without an explicit phase/space guard in `probe_read`. The current documented output (`raw=7`, normal0=4, projected=2, counts 1/0/1) supports the intended path, but the memory-stop case is less tightly asserted than the three machine-stop cases. This is a completeness concern, not evidence of a wrong result.

## 5. Failure/echo integrity

PASS:

- The memory-stop and machine-stop paths use actual `zg335116_run` output.
- `probe_machine()` only short-circuits when `stop_state` is set; the memory-stop case leaves it zero and uses the actual read callback.
- `run_memory_stop_case()` and `run_stop_case()` return non-zero on failure; `main()` accumulates failures.
- No hard-coded PASS output and no unconditional success return.

## 6. Claim boundary

PASS:

- `BEHAVIOR-PROBE.md:3` labels the result local automated-gate PASS, not runtime acceptance.
- `BEHAVIOR-PROBE.md:47-59` documents the memory-stop output and constants.
- `BEHAVIOR-PROBE.md:115-127` excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.
- ARM64 is explicitly MuMu x86_64 + ARM translation, not physical ARM.

## 7. Findings summary

| Item | Result |
|---|---|
| 0x60000 stop-read trigger | PASS; address-only trigger noted |
| raw 7 / normal 4 / projected 2 | PASS |
| counts 1/0/1 and overflow 0 | PASS |
| reason assertions | CONCERN: memory-stop reasons not asserted |
| unconditional success / fabricated output | PASS: none |
| physical ARM/runtime/SDK/OS/F4 boundary | PASS: not claimed |

Overall: **PASS (local automated behavior gates), with the memory-stop reason/phase assertion concern.** No compile, Host, device, push or runtime acceptance was performed.
