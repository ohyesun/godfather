# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Source identity checked: `harness/behavior_probe.c` SHA-256 `bf90a27e4afd14270ddc26b036c3e364fa8c00c6a40770da74e6debcd663866f` (8476 bytes), matching `BEHAVIOR-PROBE.md:64`

## 1. Verdict

**PASS for the local harness observation; CONCERNS on test-gate rigor and exact F4 setup equivalence.**

The reported x86_64/ARM64-via-MuMu `state=2/raw_state=12/dispatched=8/continued=8/normal=8/overflow=0` path is consistent with the current source and the printed actual output. This is not a runtime or platform acceptance; the report correctly keeps that boundary.

## 2. allocate_index and machine_count separation

PASS:

- `Probe` declares separate `allocated_count`, `allocate_index`, and `machine_count` at `harness/behavior_probe.c:13-16`.
- `setup()` fills the node table and increments only `allocated_count` at lines 191-192; it leaves `allocate_index` at zero.
- `probe_machine()` increments `machine_count` independently at line 122.
- Allocate uses `p->allocate_index` and increments it at lines 132-134; it never uses `allocated_count` as the cursor.
- XMM ordinal uses `changed_xmm(p->current_xmm, ordinal)` at line 122/147, with `ordinal` derived from `machine_count`, not the allocation cursor.

This directly addresses the earlier early-stop root cause described in `BEHAVIOR-PROBE-DIAGNOSIS.md:15-45`.

## 3. Memory setup and machine callback vs F4 Oracle

PASS for the normal-success path, with a qualification:

- `setup()` lines 151-219 mirrors the F4 Oracle nonempty setup: `S/D/B`, `S`/`S+4`, nine iterations of `Q/C/N0/N1`, `C` fields, zeroed `N1+36`, B offsets `{0x48,0x58,0x64,0x74,0x80,0x90,0x9c,0xac}`, and widths `{16,8,16,8,16,8,16,8}`. Compare F4 `fixture.cpp:396-437`.
- `probe_machine()` lines 118-149 mirrors the F4 Oracle normal branches: `request.entry`, continuation, allocate `eax`/`esp`, finish core return/cleanup, slot return target, length error ESP, and `changed_xmm` with `ordinal+1`. Compare F4 `fixture.cpp:591-655`.
- The harness uses the actual `zg335116_run` through `zg116la_bind_rich335` (`behavior_probe.c:236-250`); it does not call a stub that writes the expected values.
- The qualification: `setup()` is unconditionally the F4 `nonempty=true` branch. Some F4 normal tests use `Oracle::setup(false)`, which seeds `Q->Q` and only N0 nodes (`fixture.cpp:411-427`). The harness therefore matches the F4 Oracle structure and normal machine semantics, but it is not byte-for-byte equivalent to every F4 test setup. It is sufficient to exercise the observed eight-normal path; it should not be described as universal F4 Oracle equivalence.
- The harness does not reproduce every F4 error/stop/reply_transform branch; those are outside the normal path and are not used by this probe.

## 4. Early return, fabricated output, or constant-only evidence

PASS with a gate concern:

- `main()` calls `zg335116_run` once at `behavior_probe.c:250` and prints the returned `state`, `raw_result`, `normal_count`, `overflow`, and each `normal_records[i]` at lines 251-265.
- There is no early success return or hard-coded expected output. The only early return is `return 2` when `zg116la_bind_rich335` fails at lines 244-246; otherwise it prints actual returned structures.
- Concern: `main()` returns `0` unconditionally after printing (`line 266`). It does not assert `state==2`, `raw_state==12`, `normal_count==8`, or the eight normal states. Thus the harness is an observational probe, not a machine-checkable PASS gate; a wrong output would still produce exit code 0 and require a human/report comparison. This matches its role, but it should not be used as an automated acceptance test without assertions or an output checker.

## 5. Conclusion boundary

PASS:

- `BEHAVIOR-PROBE.md:3` labels the result “LOCAL HARNESS PATH PASS ... NOT RUNTIME ACCEPTANCE”.
- `BEHAVIOR-PROBE.md:71-83` explicitly excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime, and iOS.
- `BEHAVIOR-PROBE.md:42-44` correctly identifies the ARM64 result as running on an x86_64 MuMu host via ARM translation, not a physical ARM device.
- The diagnosis at `BEHAVIOR-PROBE-DIAGNOSIS.md:66-74` likewise limits the PASS to the local emulator and does not inherit F4 runtime gaps.

## 6. Findings summary

| Item | Result |
|---|---|
| allocate_index/machine_count separation | PASS |
| Memory/machine normal-path F4 alignment | PASS with branch qualification |
| Actual run vs fabricated/constant output | PASS; actual structures printed |
| Early return / test-gate rigor | CONCERN: always returns0 and has no assertions |
| ARM64 claim boundary | PASS: MuMu translation, not physical ARM |
| Runtime/SDK/OS/F4 acceptance | Not claimed; correct |

Overall: **PASS (local harness observation), with the non-blocking gate/equivalence concerns above.** No compile, Host, device, push or runtime acceptance was performed.
