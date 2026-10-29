# QA Phase 6 Cold — T82 Raw Evidence Review

- Audit date: 2026-10-30
- HEAD: `407166b`
- Raw log: `android-host-smoke/app/src/main/cpp/PHASE6-COLD-RUN.log` (1123 lines, SHA-256 `61a51300e89948ee723e7a844d56608062b5b103514d6d7125b12a7958aab168`)
- Screenshot: `PHASE6-UI-COLD-FINAL.png` (SHA-256 `33c78367ee24202b9e3bee5c1f8c4fc66ef913618d746103401775cfae100529`)
- Mode: read-only raw-log/source/artifact review; no product/tests/Java/JS changes; no push.

## 1. Verdict

**CONCERNS.**

The cold FINAL line and 36 optional failure paths are present, and the screenshot proves a real scene. However, the raw cold log does **not** contain request lines for `16777577.png`, `16786122.png`, or `16786117.png`; it also has no `MICRO-RETRY ...11_13.bin` line. Therefore `requiredFail=0` is an aggregate counter, not per-path evidence for the listed required textures or the claimed `11_13` retry. The optional-toast risk remains.

## 2. Cold FINAL and cache window

Raw single-process cold log, one `MICRO-RUN-FINAL`:

```text
MICRO-RUN-FINAL ok=1054 bytes=43539864 uniquePaths=1044 fail=37 requiredFail=0 optionalFail=36 start=1793291926544 end=1793292165856
MICRO-FAIL events=37 uniquePaths=36
SCENE-OK=0 05/0c lines=0
```

`MICRO-FAIL-REQUIRED` lines: **0**. The aggregate says no required failure, but the per-path checks below show missing required-path evidence. This is not the warm `797/fail0` window; it is a cold run.

## 3. Required-path evidence

| Path | Raw cold evidence | Status |
|---|---|---|
| `scenes/xianyangcheng/12_14.bin` | `MICRO-OK ... bytes=2363` | PASS |
| `scenes/xianyangcheng/13_13.bin` | `MICRO-OK ... bytes=2483` | PASS |
| `scenes/xianyangcheng/11_13.bin` | `MICRO-OK ... bytes=2205`; **no `MICRO-RETRY` line** | CONCERN: OK, retry claim not in raw log |
| `native107/textures/16777577.png` | **no request line** | CONCERN: required path not exercised/logged |
| `native107/textures/16786122.png` | **no request line** | CONCERN: required path not exercised/logged |
| `native107/textures/16786117.png` | **no request line** | CONCERN: required by code; direct 60548/60548 external claim not in raw log |

`PakRemote.isRequired()` classifies every `native107/textures/` path as required, so `16786117.png` cannot be labeled optional. The raw cold log has 55 texture OK lines and zero texture failure lines, but the three specifically required textures are absent from the raw log. The aggregate `requiredFail=0` therefore cannot be used as per-path proof for them.

## 4. 11199 and optional sprites

`scenes/sprites/11199.webp` appears once in the cold log:

```text
MICRO-OK path=scenes/sprites/11199.webp bytes=21106
```

It is **not** in the failure list. The user-visible failure toast for 11199 is therefore not reproduced in this cold run; it is most consistent with an older run, not the current cold log.

The exact 36 optional failures are present in `MICRO-FAIL-OPTIONAL` and all are `code=io`:

```text
scenes/sprites/9826.webp,scenes/sprites/11077.webp,scenes/sprites/10404.webp,
scenes/sprites/11074.webp,scenes/sprites/11590.webp,scenes/sprites/10979.webp,
scenes/sprites/10699.webp,scenes/sprites/10626.webp,scenes/sprites/10520.webp,
scenes/sprites/10831.webp,scenes/sprites/10691.webp,scenes/sprites/9089.webp,
scenes/sprites/8706.webp,scenes/sprites/10386.webp,scenes/sprites/10409.webp,
scenes/sprites/10218.webp,scenes/sprites/11094.webp,scenes/sprites/11191.webp,
scenes/sprites/11825.webp,scenes/sprites/10511.webp,scenes/sprites/10111.webp,
scenes/sprites/10611.webp,scenes/sprites/11081.webp,scenes/sprites/10492.webp,
scenes/sprites/11131.webp,scenes/sprites/11839.webp,scenes/sprites/14017.webp,
scenes/sprites/10025.webp,scenes/sprites/10702.webp,scenes/sprites/10692.webp,
scenes/sprites/10935.webp,scenes/sprites/9945.webp,scenes/sprites/10913.webp,
scenes/sprites/10424.webp,scenes/sprites/14023.webp,scenes/sprites/10496.webp
```

All have `code=io`. Optional failure is allowed for the resource layer, but the user-visible toast is still a **CONCERN**; these cannot be reported as zero.

## 5. UI / 6020 boundary

`PHASE6-UI-COLD-FINAL.png` shows a real scene:

- 咸阳
- 烈火教父 · 神农 · 26级
- visible scene HUD/NPCs

The raw log has `SCENE-OK=0` and `05/0c lines=0`; only `GAME-OK 6020 cmd=0a01 len=18 encrypted=des` is present. The scene is proven by the screenshot, not by a fabricated `SCENE-OK` or `05/0c` line.

## 6. Release / unchanged files

- Release APK SHA-256: `e803abbd63c33ecdd36e8adeaee4db585bcd34115d680bda049c2f05221a0917`
- Release zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- `view51.js` SHA `6fc9b01d...`, `client.js` SHA `04b5ed96...`: unchanged
- No app-tag token/password/resource-content logging found; cold log contains path/bytes/code metadata only.

## 7. Findings summary

| Item | Result |
|---|---|
| Cold FINAL 1054/43,539,864/1044/fail37 | PASS as raw cold evidence |
| requiredFail=0 aggregate | PRESENT, but per-path gaps below |
| 12_14 / 13_13 | PASS: OK |
| 11_13 | CONCERN: OK but no retry line |
| 16777577 / 16786122 / 16786117 | CONCERN: no cold request line |
| 16786117 classification | REQUIRED by code, not optional |
| 11199 | PASS: OK 21106; toast not reproduced |
| 36 optional failures | CONCERN: allowed, but visible toast risk; not 0 |
| UI scene screenshot | PASS |
| SCENE-OK / 05/0c | NOT OBSERVED; not fabricated |
| release p12 | PASS: 0 |
| view51/client/protocol | PASS: unchanged |

## 8. Boundary / T83

T83's all-sprite retry is not signed by this report. This review does not claim optional toast is eliminated; it only records the current cold evidence. A cold rerun with per-path required logging and exact optional path/code capture is still required to promote requiredFail=0 to a per-path PASS.

This is local WebView/APK QA evidence on MuMu x86_64. No product source, tests, Java or JS was modified; no push was performed.
