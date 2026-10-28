# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `e63ecc13661f5027ec8d05ea4e90f7e4e6f39cfbf9fa84e27b045673f0059e14` (22414 bytes), matching `BEHAVIOR-PROBE.md:140`
- Binary identities on disk match:
  - x86_64 `build-local-x86_64/zg335_behavior_probe` SHA-256 `f01a0137cea2a5c87ad1bfff0ec80bfb226b8d8dc4a1037dc4f683ef1842b411`
  - arm64-v8a `out/behavior-probe-arm64-v8a` SHA-256 `d6a48523c7e834c89b7657b8fa1ca95746bb703c26676ed85874349b48306d2a`
- Documentation identity: `BEHAVIOR-PROBE.md` SHA-256 `831896a0572a673ed1d589514a07aec277e4f4873cfbaa06074c2551e59a258b`.

## 1. Verdict

**PASS for the current malformed-width16 execution path; CONCERNS for assertion coverage relative to F4 `test_vector_malformed`.**

The harness uses a real one-shot malformed read at RAM address `0x00030048` (B+0x48) with width 16, and checks the actual raw result `MEMORY_REPLY_INVALID` / `READ_INITIALIZATION`, eight normal records, last address and width. No fabricated output or unconditional success path was found. However, the harness does not assert the XMM-preservation condition present in F4 `test_vector_malformed`, and does not assert `continued=8` even though that value is in the reported output.

## 2. Malformed trigger

- `Probe` carries `malformed_read_address`, `malformed_read_width`, and `malformed_read_hit` at `harness/behavior_probe.c:24-26`.
- `run_malformed_case()` sets:
  - `malformed_read_address = 0x00030048U` (B+0x48) at line 473;
  - `malformed_read_width = 16U` at line 474.
- `probe_read()` applies the malformed initialized mask only when:
  - `space == ZG335_SPACE_RAM`;
  - address equals `malformed_read_address`;
  - width equals `malformed_read_width`;
  - `malformed_read_hit` is zero.
  See lines 101-106. It sets `initialized_mask32` to `0xff` for width16, then sets `malformed_read_hit = 1`.
- The trigger is one-shot and does not fabricate raw output; the actual read reply is consumed by `zg335116_run` at line 490.

## 3. Assertions and constants

`run_malformed_case()` is at `behavior_probe.c:460-511`; the predicate at lines 501-508 checks:

- `state == ZG335116_HOST_COMPLETED`
- `normal_count32 == 8`
- `record_overflow32 == 0`
- raw `state32 == ZG335_MEMORY_REPLY_INVALID` (6)
- raw `reason32 == ZG335_REASON_READ_INITIALIZATION` (5)
- `calls_dispatched32 == 8`
- `last_memory.address32 == 0x00030048`
- `last_memory.width32 == 16`

The constants match the design headers:

- `ZG335_MEMORY_REPLY_INVALID = 6`
- `ZG335_REASON_READ_INITIALIZATION = 5`

The result is printed at line 509 and returned as `ok ? 0 : 1` at line 510; `main()` ORs it into `failed` at line 593 and returns `failed` at line 594. There is no unconditional success return.

## 4. F4 `test_vector_malformed` coverage

F4 `test_vector_malformed()` at `fixture.cpp:1732-1758` checks:

- normal count 8 and dispatched 8;
- raw `MEMORY_REPLY_INVALID`;
- raw `READ_INITIALIZATION`;
- last memory address/width;
- XMM preserved after the failed read (`fixture.cpp:1752-1757`).

The harness covers the address/width/state/reason/count portion, but:

- **CONCERN 1:** it does not assert `output.raw_result.xmm0` equals the preserved F4 value; a malformed read could alter XMM and still pass the current harness.
- **CONCERN 2:** it does not assert `calls_continued32 == 8`, although the reported output contains `continued=8` and F4's success path expects eight continuations. The harness only checks `dispatched=8` and `normal=8`.
- F4 also exercises a width8 malformed vector (`fixture.cpp:1734-1743`); the task here is width16, so width8 coverage is a scope note, not a current failure.
- The raw `last_memory_present`/kind/space fields are not asserted; F4's `test_vector_malformed` expects address/width only, so this is a minor coverage note.

## 5. Integrity and failure behavior

- The trigger is a real read callback side effect, not a hard-coded output.
- `RESULT PASS/FAIL` is printed after the predicate; a failed predicate returns non-zero.
- `main()` accumulates the malformed result with `failed |= run_malformed_case()`; no malformed path is silently ignored.
- The normal, nonreturn, memory-stop and stale-continuation gates remain present; the malformed case uses empty setup, while the normal path covers empty/nonempty.

## 6. Claim boundary

- `BEHAVIOR-PROBE.md:3` labels the result local automated-gate PASS, not runtime acceptance.
- `BEHAVIOR-PROBE.md:79-91` documents the malformed width16 output and constants.
- `BEHAVIOR-PROBE.md:147-159` excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.
- ARM64 is explicitly MuMu x86_64 + ARM translation, not physical ARM.

## 7. Findings summary

| Item | Result |
|---|---|
| B+0x48 width16 one-shot trigger | PASS |
| raw6 / reason5 / last address / width | PASS |
| 8 normal records / dispatched8 | PASS |
| continued8 assertion | CONCERN: missing |
| F4 XMM-preservation assertion | CONCERN: missing |
| width8 malformed vector | Scope note; not requested here |
| fabricated output / unconditional success | PASS: none |
| runtime/physical ARM/SDK/OS/F4 boundary | PASS: not claimed |

Overall: **PASS for the observed malformed-width16 path; CONCERNS on XMM and continued-count assertion coverage.** No compile, Host, device, push or runtime acceptance was performed.
