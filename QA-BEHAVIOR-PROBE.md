# Behavior Probe Independent QA

- Audit date: 2026-10-28
- Scope: `harness/behavior_probe.c`, `BEHAVIOR-PROBE.md`, `harness/BEHAVIOR-PROBE-DIAGNOSIS.md`
- Mode: read-only static review; no compile, Host run, device run or push
- Current source identity: `harness/behavior_probe.c` SHA-256 `4f91d2b185c23d8b1ec6cd6c10d7c9c4fe158bc82b632a6e88fdedcff6852015` (22815 bytes), matching `BEHAVIOR-PROBE.md:140`
- Binary identities match the current file tree and documentation:
  - x86_64 `build-local-x86_64/zg335_behavior_probe` SHA-256 `4a3b84e6a048d5af9bc03f1c9f5659f2c6bc5046a520be9d30f0102970609f31` (344344 bytes)
  - arm64-v8a `out/behavior-probe-arm64-v8a` SHA-256 `515b2412734614a64fe24cb3aa15ccc104d474ca06228eb1f1ccb79ead499fad` (366560 bytes)
- Documentation identity: `BEHAVIOR-PROBE.md` SHA-256 `e9a9957c3e764161269502b15ef53a155078f7f1f367cb124e2a8b068da17e0e`.
- Previous hash conflict is resolved: `BEHAVIOR-PROBE.md:140-144` pin these exact source/x86/arm64 identities.

## 1. Verdict

**PASS for the current local automated behavior gates; not a runtime or platform acceptance.**

The malformed-width16 path now closes both prior assertion concerns:

- `calls_continued32 == 8` at `behavior_probe.c:507`;
- raw XMM compared to `normal_records[7].normal_result.xmm0` in all four words at lines 510-513, aligning with F4 `normals[7]` rather than the harness's internal `probe.current_xmm`.

The malformed trigger remains a real one-shot read at B+0x48 with width16; the path uses actual `zg335116_run`; no fabricated output or unconditional success was found.

## 2. Malformed trigger and assertion

- `malformed_read_address`, `malformed_read_width`, `malformed_read_hit` at `behavior_probe.c:24-26`.
- `run_malformed_case()` sets `0x00030048` (B+0x48) and width16 at lines 473-474.
- `probe_read()` applies the malformed initialized mask only for RAM/address/width/one-shot at lines 101-106; width16 mask is `0xff`.
- The predicate at lines 501-513 checks:
  - host `ZG335116_HOST_COMPLETED`;
  - `normal_count32 == 8`, `record_overflow32 == 0`;
  - raw `ZG335_MEMORY_REPLY_INVALID` (6), reason `ZG335_REASON_READ_INITIALIZATION` (5);
  - dispatched 8 and continued 8;
  - last memory address `0x00030048`, width 16;
  - raw XMM equals `normal_records[7].normal_result.xmm0` on all four words.
- `RESULT PASS/FAIL` is after the predicate; failure returns non-zero. `main()` ORs the result into `failed`.

## 3. F4 `test_vector_malformed` coverage

F4 `test_vector_malformed()` at `fixture.cpp:1732-1758` checks normal/dispatched, raw `MEMORY_REPLY_INVALID`/`READ_INITIALIZATION`, last address/width, and XMM preservation against `normals[7]`. The current harness covers the same normal/dispatched/raw/address/width fields, adds `continued8`, and uses the explicit `normal_records[7]` XMM reference. The prior XMM-reference concern is resolved.

F4 also exercises width8; this task is width16, so width8 remains a scope note. The malformed case uses empty setup; the normal 8-record path covers empty/nonempty.

## 4. Integrity and boundary

- The malformed mask is produced by the real read callback, not hard-coded output.
- `malformed_read_hit` is reset per case and makes the trigger one-shot.
- The case returns non-zero on predicate failure and is accumulated by `main()`.
- `BEHAVIOR-PROBE.md:3` labels the result local automated-gate PASS, not runtime acceptance.
- `BEHAVIOR-PROBE.md:147-159` excludes physical ARM64, production runtime, SDK/OS/native ABI, Host/F4 full runtime and iOS.
- ARM64 remains MuMu x86_64 + ARM translation, not physical ARM.

## 5. Findings summary

| Item | Result |
|---|---|
| B+0x48 width16 one-shot trigger | PASS |
| raw6 / reason5 / last addr/width | PASS |
| dispatched8 / continued8 / normal8 | PASS |
| XMM == normal_records[7] four words | PASS |
| documented artifact pins | PASS: source/x86/arm64 match current files |
| fabricated output / unconditional success | PASS: none |
| runtime/physical ARM/SDK/OS/F4 boundary | PASS: not claimed |

Overall: **PASS (local automated behavior gates), limited to the documented normal, forced-machine-nonreturn, memory-stop, stale-continuation and malformed-width16 paths.** No compile, Host, device, push or runtime acceptance was performed.
