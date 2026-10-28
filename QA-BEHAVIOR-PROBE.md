# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `608ee1edbdb432a174d7e50165ae6266d8a68f8858341efee34ad4f15937fc2b` (19870 bytes), matching `BEHAVIOR-PROBE.md:126`
- Binary identities on disk match:
  - x86_64 `build-local-x86_64/zg335_behavior_probe` SHA-256 `2c86eecf326e46f579f9e347f4ab4128025cce5e0b56d7847620de6ff15b806d`
  - arm64-v8a `out/behavior-probe-arm64-v8a` SHA-256 `28c240f4e41cf1385875499e135a5142cd668b21585fe18e0694c98f582600fe`
- Documentation identity: `BEHAVIOR-PROBE.md` SHA-256 `0d0053b09498ba619a44861b46b7e785fc0f206b78916b4832cf78cb2109a88a`.

## 1. Verdict

**PASS for the current local automated behavior gates; not a runtime or platform acceptance.**

The stale-continuation path is present with the expected first-machine-seam write and a full output predicate. The harness retains the normal 8-record, forced-machine-nonreturn, and memory-stop gates. No fabricated output or unconditional success path was found.

## 2. Stale continuation trigger

- `Probe` carries `stale_address` and `stale_value` at `harness/behavior_probe.c:22-23`.
- `run_stale_case()` sets:
  - `stale = 0x2468ace0U` at line 385;
  - `probe.stale_address = 0x0000ffe0U` at line 391 (S is `0x00010000`, so this is S-0x20);
  - `probe.stale_value = stale` at line 392.
- `probe_machine()` writes `stale_value` to `stale_address` only when `machine_count == 0` at lines 137-139, before incrementing the machine ordinal at line 140. This is the first machine seam.
- The case uses the empty setup at line 390 and the actual `zg335116_run` at line 408; no stub substitution.

## 3. Stale assertions and constants

- `run_stale_case()` is at `behavior_probe.c:377-448`.
- The predicate at lines 430-446 checks:
  - host `ZG335116_HOST_COMPLETED`
  - normal count 1, overflow 0
  - normal0 `ZG116_RETURNED` (13), reason 0
  - normal final target present 1, target `0x2468ace0`, cleanup 4
  - projected `ZG335_LOWER_RETURNED` (1), reason 0
  - projected `returned_to32` `0x2468ace0`, `ret_cleanup32` 4
  - raw `ZG335_RETURN_CONTROL_INVALID` (11), reason `ZG335_REASON_RETURN_TARGET` (9)
  - dispatched 1, continued 0
  - raw final-target-present 0
- Constants match the design headers: raw 11/reason 9, normal 13, projected 1, target `0x2468ace0`, cleanup 4.
- The predicate contains no hard-coded output values; it reads the actual `output` structure.

## 4. Failure and coverage integrity

- The only early return is a genuine bind failure at lines 402-405.
- `RESULT PASS/FAIL` is printed after the predicate at line 447; `return ok ? 0 : 1` is at line 448.
- `main()` ORs the stale result into `failed` at line 530 and returns `failed` at line 531; there is no unconditional success.
- Normal path retains empty/nonempty setup and eight-record validation.
- Forced-machine nonreturn retains STOPPED/INVALID/UNKNOWN state/reason/count checks.
- Memory-stop retains `stop_read_armed`, RAM/0x60000/width4/one-shot trigger, and raw/normal/projected reason checks.
- The stale case uses empty setup, and the memory-stop case also uses empty setup; both normal setup branches are covered only by the normal 8-record path. This is a scope note, not a failure.

## 5. Claim boundary

PASS:

- `BEHAVIOR-PROBE.md:3` labels the result local automated-gate PASS, not runtime acceptance.
- `BEHAVIOR-PROBE.md:61-77` documents the stale continuation mapping and expected values.
- `BEHAVIOR-PROBE.md:133-145` excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.
- ARM64 is explicitly MuMu x86_64 + Android ARM translation, not physical ARM.

## 6. Findings summary

| Item | Result |
|---|---|
| first machine seam writes S-0x20 = 0x2468ace0 | PASS |
| raw 11 / reason 9 / counts 1-0-1 | PASS |
| normal 13 / final target / cleanup 4 | PASS |
| projected 1 / returned_to 0x2468ace0 / cleanup 4 | PASS |
| fabricated output/unconditional success | PASS: none |
| stale empty setup coverage | Note; not a current failure |
| runtime/physical ARM/SDK/OS/F4 boundary | PASS: not claimed |

Overall: **PASS (local automated behavior gates), limited to the documented normal, forced-machine-nonreturn, memory-stop and stale-continuation paths.** No compile, Host, device, push or runtime acceptance was performed.
