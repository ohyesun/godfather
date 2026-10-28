# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `49ed3f412ae683cd86b6ff4c26752d6a7a25553c61f104cdae76076dbbd78562` (9755 bytes), matching `BEHAVIOR-PROBE.md:75`
- Binary identities on disk match the documentation: x86_64 `06d95f35...`, arm64-v8a `500ab46e...`

## 1. Verdict

**PASS for the current local automated harness gate; not a runtime or platform acceptance.**

The current harness runs both `empty` and `nonempty` setup modes, executes the actual `zg335116_run`, verifies the required host/raw/normal counters and all eight normal states, prints `RESULT PASS/FAIL`, and returns non-zero on failure. The reported MuMu x86_64 and ARM64-via-translation `state=2 / raw_state=12 / dispatched=8 / continued=8 / normal=8 / overflow=0 / normal[*]=13` path is consistent with the source and documentation.

Remaining boundary: this is still a local normal-success behavior observation. It does not establish physical ARM64, production runtime, SDK/OS/native ABI, signing, device, Host/F4 full runtime or iOS acceptance. The docs keep that boundary.

## 2. allocate_index and machine_count separation

PASS:

- `Probe` has independent `allocated_count`, `allocate_index`, and `machine_count` at `harness/behavior_probe.c:13-16`.
- `setup()` fills the node table and increments only `allocated_count` at lines 190-211; `allocate_index` starts at zero after the per-mode `memset` at `harness/behavior_probe.c:242`.
- `probe_machine()` increments `machine_count` at line 122 and uses it for the XMM ordinal.
- Allocate uses `p->allocate_index` and increments it at lines 132-134; it never uses `allocated_count` as the cursor.

## 3. Memory setup and machine callback vs F4 Oracle

PASS for both F4 normal setup branches:

- `setup()` now takes `nonempty` at line 151 and reproduces both F4 Oracle branches:
  - empty: `Q->Q`, N0-only nodes at lines 208-210;
  - nonempty: `Q->C`, N0/N1 nodes, C fields and N1 zeroing at lines 190-207.
  Compare F4 `fixture.cpp:396-437`.
- `probe_machine()` lines 118-149 mirrors the F4 Oracle normal branches: return image/continuation, allocate `eax`/`esp`, finish core return/cleanup, slot return target, length-error ESP, and `changed_xmm` with `ordinal+1`. Compare F4 `fixture.cpp:591-655`.
- The harness calls the actual `zg335116_run` through `zg116la_bind_rich335` at `harness/behavior_probe.c:246-250`; it does not stub the library or write expected output.

The harness still does not reproduce F4's stop/reply-transform/error branches; those are outside this normal-path probe.

## 4. Automated gate and output integrity

PASS:

- `main()` runs both `nonempty=0/1` modes at lines 233-244.
- It prints the actual returned state, raw result, counts, overflow, and all normal records at lines 261-275.
- The `ok` predicate at lines 278-283 checks:
  - `ZG335116_HOST_COMPLETED`
  - `ZG335_RETURNED`
  - `calls_dispatched32 == 8`
  - `calls_continued32 == 8`
  - `normal_count32 == 8`
  - `record_overflow32 == 0`
- Lines 284-289 require all eight `normal_records[i].normal_result.state32` to be `ZG116_RETURNED`.
- `RESULT PASS/FAIL` is printed only after those checks at lines 290-291; `failed` is set on any failure and returned at lines 292-296, so a wrong result exits non-zero.
- There is no fabricated constant output and no unconditional success return. The only early return is the genuine bind failure at lines 254-256.

Minor boundary note: the gate checks the requested normal states and counters, but not `reason==0` or the displayed `eax=00020000`; those are documentation output values, not part of the asserted gate. The documented PASS is therefore a valid local gate for its stated criteria, not a full field-by-field F4 equivalence check.

## 5. Conclusion boundary

PASS:

- `BEHAVIOR-PROBE.md:3` labels the result “LOCAL HARNESS AUTOMATED GATE PASS ... NOT RUNTIME ACCEPTANCE”.
- `BEHAVIOR-PROBE.md:82-95` excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.
- `BEHAVIOR-PROBE.md:53-55` correctly identifies MuMu as x86_64 running the ARM64 binary through ARM translation, not a physical ARM device.
- Both setup modes are exercised; the earlier `main` always-return-0 concern is resolved.

## 6. Findings summary

| Item | Result |
|---|---|
| allocate_index/machine_count separation | PASS |
| Empty and nonempty F4 Oracle setup alignment | PASS |
| Actual run vs fabricated/constant output | PASS |
| Early return / failed-result exit status | PASS; returns non-zero on failure |
| Normal gates and eight-record check | PASS |
| Runtime/ARM64/SDK/OS/F4 acceptance boundary | PASS; correctly excluded |

Overall: **PASS (local automated harness gate), limited to the documented normal-success behavior.** No compile, Host, device, push or runtime acceptance was performed.
