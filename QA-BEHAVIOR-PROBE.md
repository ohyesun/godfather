# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `f92ccf5943de87dcb0010d1fca5bc9425a2921bbfca9d3f91a3ab6d35467791e` (12965 bytes), matching `BEHAVIOR-PROBE.md:94`
- Binary identities on disk match:
  - x86_64 `build-local-x86_64/zg335_behavior_probe` SHA-256 `f11cd7d06b11be01ccaa3e1578d7f53e6afabfc6f5976207d45a53d04bbd2766`
  - arm64-v8a `out/behavior-probe-arm64-v8a` SHA-256 `cab32d4c7f1e6c9a774b76eb7947fb3250e8048441480d030cb7b7c30935fd22`
- Documentation identity: `BEHAVIOR-PROBE.md` SHA-256 `cfa4a0a614be6915d88f1d85e6f0ff7ea93be9f6c983982f542e115361c70aa7`.

## 1. Verdict

**PASS for the current local automated harness gates; not a runtime or platform acceptance.**

The harness retains the 8-normal empty/nonempty checks and adds three nonreturn cases. The source checks are real data comparisons against `zg335116_run` output, not hard-coded PASS values. The observed MuMu x86_64/ARM64-translation results are consistent with the source and documented matrix.

Coverage qualification: the stop matrix calls `setup(&probe, &entry, 0)` (`behavior_probe.c:245`), so it covers the empty setup only; both empty and nonempty setup branches are covered by the normal 8-record path. This is a scope limitation, not a current failure.

## 2. Normal-path gate retained

PASS:

- `main()` loops `nonempty=0/1` at `behavior_probe.c:300-311`.
- The normal gate checks host COMPLETED, raw RETURNED, raw reason 0, EAX `0x00020000`, dispatched/continued/normal 8, overflow 0, and all eight normal states/reasons at lines 345-359.
- `RESULT PASS/FAIL` is printed after the checks, and failure propagates through `failed`.
- `allocate_index`, `allocated_count`, and `machine_count` remain independent at lines 13-17 and probe lines 123-139.

## 3. Nonreturn matrix checks

PASS for the requested three cases:

`run_stop_case()` is at `behavior_probe.c:232-292`; it seeds empty setup, sets `probe.stop_state`, calls actual `zg335116_run`, prints the actual output, and returns 0 only when all checks at lines 278-288 hold.

`main()` invokes:

- `run_stop_case(ZG335_SERVICE_STOPPED, ZG335_CALLEE_STOPPED, ZG116_CALLEE_STOPPED, ZG335_LOWER_STOPPED)` at lines 366-367.
- `run_stop_case(ZG335_SERVICE_INVALID, ZG335_CALLEE_INVALID, ZG116_CALLEE_INVALID, ZG335_LOWER_INVALID)` at lines 368-369.
- `run_stop_case(ZG335_SERVICE_UNKNOWN, ZG335_CALLEE_UNKNOWN, ZG116_CALLEE_UNKNOWN, ZG335_LOWER_UNKNOWN)` at lines 370-371.

Constant mapping is correct against `design/include/zg335/zg_copy335_raw_normal_frame.h` and `zg_normal116_fullstate.h`:

| service | raw expected | normal expected | projected expected | assert counts |
|---|---|---|---|---|
| STOPPED 2 | CALLEE_STOPPED 7 | CALLEE_STOPPED 8 | LOWER_STOPPED 2 | dispatched 1 / continued 0 / normal 1 |
| INVALID 3 | CALLEE_INVALID 8 | CALLEE_INVALID 9 | LOWER_INVALID 3 | 1 / 0 / 1 |
| UNKNOWN 4 | CALLEE_UNKNOWN 9 | CALLEE_UNKNOWN 10 | LOWER_UNKNOWN 4 | 1 / 0 / 1 |

`run_stop_case()` checks `normal_records[0]` only after requiring `normal_count32 == 1` in the `ok` predicate; the fixed record slot 0 exists, so there is no out-of-bounds access. It also checks raw reason 0, normal reason 0, projected reason 0, and overflow 0. The stop path in `probe_machine()` is the genuine callback return at lines 125-128; no canned output is injected.

## 4. Failure/echo integrity

PASS:

- `probe_machine()` increments `machine_count` and returns `stop_state` when set; otherwise it runs the actual normal seam logic.
- `run_stop_case()` returns 0 only on the full predicate; otherwise returns 1.
- `main()` ORs each stop-case result into `failed` and returns `failed` at line 372.
- There is no unconditional success return and no hard-coded state/count output. The printed values come from `output`.
- The only early return is bind failure (`return 2`), which is a genuine setup failure.

## 5. Claim boundary

PASS:

- `BEHAVIOR-PROBE.md:3` labels the result as a local automated-gate PASS, not runtime acceptance.
- `BEHAVIOR-PROBE.md:33-45` documents the nonreturn matrix and its asserted counts/states/reasons.
- `BEHAVIOR-PROBE.md:101-113` excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.
- ARM64 results are explicitly MuMu x86_64 + ARM translation, not physical ARM.

## 6. Findings summary

| Item | Result |
|---|---|
| STOPPED/INVALID/UNKNOWN mapping | PASS |
| raw/normal/projected checks | PASS |
| counts 1/0/1 and reasons | PASS |
| out-of-bounds | PASS: record slot 0 valid; normal_count==1 required |
| fabricated output/unconditional success | PASS: actual run and full predicates |
| both setup branches | Normal path yes; stop matrix empty-only (coverage note) |
| runtime/platform boundary | PASS: not claimed |

Overall: **PASS (local automated behavior gates), with the stop-matrix empty-setup coverage note.** No compile, Host, device, push or runtime acceptance was performed.
