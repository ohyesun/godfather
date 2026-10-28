# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `f9a3b3c53bbb8438ded537170d84603f22eac5b1cc5bb61f074852c150f387b0` (22711 bytes), matching `BEHAVIOR-PROBE.md:140`
- Binary identities on disk:
  - x86_64 `build-local-x86_64/zg335_behavior_probe` SHA-256 `46bc7e88d1be20715c7c7b4e3d584052ee0dd8243068a4f33fe013720c8548b4` (matches doc)
  - arm64-v8a `out/behavior-probe-arm64-v8a` SHA-256 `a16b20c998d4beea380d213e67607590ada5bcf4ea81e1d46b202c9e4207eb7f` (**does not match `BEHAVIOR-PROBE.md:144`, which still pins `40344f91...`**)
- Documentation identity: `BEHAVIOR-PROBE.md` SHA-256 `24f7a7f58e2c518264319b8a8c064ff4109f10c87ef9c42b85336dce9edccef7`.

## 1. Verdict

**PASS for the malformed-width16 execution path and the added continued/XMM checks; CONCERNS for exact F4 XMM coverage and a stale ARM64 artifact pin.**

The current code adds `calls_continued32 == 8` and raw XMM comparisons. The malformed path is real, uses `zg335116_run`, and returns non-zero on failure. No fabricated output or unconditional success was found.

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
  See lines 101-106; it sets `initialized_mask32 = 0xff` for width16 and sets the one-shot hit flag.
- `run_malformed_case()` uses the empty setup at line 472 and calls actual `zg335116_run` at line 490.

## 3. Assertions added

`run_malformed_case()` is at `behavior_probe.c:460-515`; the predicate at lines 501-513 checks:

- host `ZG335116_HOST_COMPLETED`
- `normal_count32 == 8`, `overflow == 0`
- raw `ZG335_MEMORY_REPLY_INVALID` (6), reason `ZG335_REASON_READ_INITIALIZATION` (5)
- dispatched 8 and **continued 8** (line 507)
- last memory address `0x00030048`, width 16
- raw XMM words equal `probe.current_xmm` words (lines 510-513)

The result is printed at line 514 and returned as `ok ? 0 : 1` at line 515; `main()` ORs it into `failed` at line 598 and returns at line 599. There is no unconditional success.

## 4. F4 `test_vector_malformed` coverage

F4 `test_vector_malformed()` at `fixture.cpp:1732-1758` checks normal/dispatched, raw state/reason, last address/width, and **XMM preserved** (`fixture.cpp:1752-1757`). The current harness covers the state/reason/count/address/width portion and now checks XMM, but with two qualifications:

- **CONCERN 1 — XMM reference:** the harness compares `output.raw_result.xmm0` to `probe.current_xmm`, the harness's own callback state. F4 compares to `normals[7].result.xmm0` (or the initial B words for the width16 case). `probe.current_xmm` is not an independently pinned `normal_records[7]` value, so this is not yet a fully explicit F4-reference check.
- **CONCERN 2 — stale documentation pin:** `BEHAVIOR-PROBE.md:144` still lists arm64-v8a `40344f91...`, while the actual file now hashes to `a16b20c9...`. Source and x86 pins match; the ARM64 artifact pin is stale. This is an evidence/provenance mismatch, not a behavior failure.
- F4 also covers a width8 malformed vector; this task is width16, so width8 is a scope note.

## 5. Integrity and boundary

- The malformed mask is produced by the real one-shot read callback, not hard-coded output.
- `malformed_read_hit` is reset by the per-case `memset`; the trigger is one-shot.
- The case returns non-zero on any predicate failure and is ORed into `main`'s `failed`.
- `BEHAVIOR-PROBE.md:3` labels the result a local automated-gate PASS, not runtime acceptance.
- `BEHAVIOR-PROBE.md:147-159` excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.
- ARM64 remains MuMu x86_64 + ARM translation, not physical ARM.

## 6. Findings summary

| Item | Result |
|---|---|
| B+0x48 width16 one-shot trigger | PASS |
| raw6 / reason5 / last addr/width | PASS |
| dispatched8 / continued8 / normal8 | PASS |
| XMM preservation check | CONCERN: compared to harness `probe.current_xmm`, not explicit F4 `normals[7]` |
| ARM64 artifact pin | CONCERN: doc `40344f91...` vs actual `a16b20c9...` |
| fabricated output / unconditional success | PASS: none |
| runtime/physical ARM/SDK/OS/F4 boundary | PASS: not claimed |

Overall: **PASS for the malformed-width16 execution path; CONCERNS on exact F4 XMM-reference coverage and the stale ARM64 artifact pin.** No compile, Host, device, push or runtime acceptance was performed.
