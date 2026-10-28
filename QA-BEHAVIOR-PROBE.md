# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `42c48076d767c3577f3a8febaa1c6c7b185ffe43d84dc02f41dfe36fc9bbbd14` (9933 bytes), matching `BEHAVIOR-PROBE.md:80`
- Binary identities on disk match the documentation:
  - x86_64 `build-local-x86_64/zg335_behavior_probe` SHA-256 `49608d6328616c867db7bdefba5201dfda0afd7f94e7f5f9e8135978f40973bc`
  - arm64-v8a `out/behavior-probe-arm64-v8a` SHA-256 `ffcc92281fd6ede812ec0e66d9d61480729b326806898c7d474369eeadbe92a0`
- Documentation identity: `BEHAVIOR-PROBE.md` SHA-256 `346e1bfb726b7840b295ad6545b1e85f79ec998a2f4a0cdb6ad92ab22c7223f4`; diagnosis `d975bf67...`.

## 1. Verdict

**PASS for the current local automated harness gate; not a runtime or platform acceptance.**

The current harness runs both `empty` and `nonempty` setup modes, executes the actual `zg335116_run`, verifies the requested host/raw/normal counters, raw `reason=0`/`eax=0x00020000`, and every normal record's `state=13`/`reason=0`; it prints `RESULT PASS/FAIL` and returns non-zero on failure. The reported MuMu x86_64 and ARM64-via-translation outputs are consistent with the source and are bounded as local harness results.

## 2. allocate_index and machine_count separation

PASS:

- `Probe` has independent `allocated_count`, `allocate_index`, and `machine_count` at `harness/behavior_probe.c:13-16`.
- `setup()` fills the node table and increments only `allocated_count` at lines 190-211; the per-mode `memset` resets `allocate_index` at line 242.
- `probe_machine()` increments `machine_count` at line 122 and uses it for the XMM ordinal.
- Allocate uses `p->allocate_index` and increments it at lines 132-134; it never uses `allocated_count` as the cursor.

## 3. Memory setup and machine callback vs F4 Oracle

PASS for both F4 normal setup branches:

- `setup()` takes `nonempty` at line 151 and reproduces both F4 Oracle branches:
  - empty: `Q->Q`, N0-only at lines 208-210;
  - nonempty: `Q->C`, N0/N1 nodes, C fields and N1 zeroing at lines 190-207.
  Compare F4 `fixture.cpp:396-437`.
- `probe_machine()` at lines 118-149 mirrors the F4 Oracle normal branches: actual return image/continuation, allocate `eax`/`esp`, finish core return/cleanup, slot return target, length-error ESP, and `changed_xmm` with `ordinal+1`. Compare F4 `fixture.cpp:591-655`.
- The harness calls the actual `zg335116_run` through `zg116la_bind_rich335` at `harness/behavior_probe.c:246-250`; it does not stub the library or write expected output.

The harness does not reproduce F4's stop/reply-transform/error branches; those are outside this normal-path probe.

## 4. Automated gate and output integrity

PASS:

- `main()` runs both `nonempty=0/1` modes at lines 233-244.
- It prints the actual returned state, raw result, counts, overflow, and all normal records at lines 261-275.
- The `ok` predicate at lines 278-285 checks:
  - `ZG335116_HOST_COMPLETED`
  - `ZG335_RETURNED`
  - raw `reason32 == 0`
  - raw `regs.eax == 0x00020000`
  - `calls_dispatched32 == 8`
  - `calls_continued32 == 8`
  - `normal_count32 == 8`
  - `record_overflow32 == 0`
- Lines 286-291 require all eight normal records to have `state32 == ZG116_RETURNED` and `reason32 == 0`.
- `RESULT PASS/FAIL` is printed only after those checks at lines 293-294; any failure sets `failed` at lines 295-297 and is returned at lines 298-299.
- There is no fabricated constant output and no unconditional success return. The only early return is the genuine bind failure at lines 254-256.

The gate remains a normal-success-path probe: it does not check F4 stop/error/reply-transform behavior, and it does not replace a full field-by-field Oracle comparison.

## 5. Conclusion boundary

PASS:

- `BEHAVIOR-PROBE.md:3` labels the result “LOCAL HARNESS AUTOMATED GATE PASS ... NOT RUNTIME ACCEPTANCE”.
- `BEHAVIOR-PROBE.md:29-45` documents both empty and nonempty modes and the exact asserted criteria, including raw reason/eax and normal reasons.
- `BEHAVIOR-PROBE.md:58-60` correctly identifies MuMu as x86_64 running the ARM64 binary through ARM translation, not a physical ARM device.
- `BEHAVIOR-PROBE.md:87-99` excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.

## 6. Findings summary

| Item | Result |
|---|---|
| allocate_index/machine_count separation | PASS |
| Empty and nonempty F4 Oracle setup alignment | PASS |
| Actual run vs fabricated/constant output | PASS |
| Early return / failed-result exit status | PASS; returns non-zero on failure |
| Raw reason/eax and normal reason gates | PASS |
| Normal counters and eight-record check | PASS |
| Runtime/ARM64/SDK/OS/F4 acceptance boundary | PASS; correctly excluded |

Overall: **PASS (local automated harness gate), limited to the documented normal-success behavior.** No compile, Host, device, push or runtime acceptance was performed.
