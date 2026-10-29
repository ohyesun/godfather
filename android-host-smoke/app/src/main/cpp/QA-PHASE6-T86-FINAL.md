# QA Phase 6 T86 Final Adjudication

- Audit date: 2026-10-30
- HEAD: `f69de37` (pushed)
- Raw log: `android-host-smoke/app/src/main/cpp/PHASE6-COLD-RUN-T86.log` (1268 lines, SHA-256 `6c1c3a4aa84db7510fa56285a5485716edf7fa7217a055472f3f1e249eb45a24`)
- Screenshot: `PHASE6-UI-T86.png` (SHA-256 `de404e0b1de89bb80c85a1f7c73de20caa883644b8f3c350e3c7ac67e9f55e72`)
- Mode: raw-log/source/artifact review; no product/tests/Java/JS changes; no push.

## 1. Verdict

**PASS with CONCERNS — 功能进图 PASS，时序窗口 CONCERN。**

The 87/87 required-path evidence is complete and `fail=0` at the emitted FINAL; `8277` and `11199` are both finally OK, and the screenshot has no visible error toast. However, FINAL is a 180001ms process-timeout snapshot, not HUD+30s, and optional retry/recovery continues after FINAL. The FINAL counts are therefore not a complete post-retry final-state accounting.

## 2. 87/87 required paths

Raw log parsing:

```text
MICRO-SCOPE map=xianyangcheng spawn=138,184 duration=30s paths=87 sha256=4a3a5015
MICRO-REQ: 87
status=ok: 87
status=absent: 0
MICRO-FAIL-REQUIRED: absent
requiredFail=0
```

Required examples:

- `native107/textures/16786117.png`: `MICRO-OK ... bytes=60548` plus `MICRO-REQ ... status=ok attempts=1`
- `scenes/xianyangcheng/11_13.bin`: `MICRO-RETRY attempt=2`, `MICRO-RETRY attempt=3`, `MICRO-RECOVERED attempts=3`, `MICRO-OK bytes=2205`, final `MICRO-REQ status=ok attempts=3`
- all other required textures and `scenes/xianyangcheng/*.bin` / `map.json`: `status=ok`

The 87/87 and `requiredFail=0` claims are supported.

## 3. Optional 8277 / 11199 and toast

- `scenes/sprites/8277.webp`: `MICRO-OK bytes=10399`; no failure event in this T86 log.
- `scenes/sprites/11199.webp`: `MICRO-OK bytes=21106`; no failure event.
- Screenshot `PHASE6-UI-T86.png` shows the actual scene and no visible “场景贴图读取失败” toast.

Both are finally OK in this run.

## 4. FINAL and post-FINAL retries

Exact FINAL:

```text
MICRO-RUN-FINAL ok=936 bytes=40099557 uniquePaths=918 fail=0 requiredFail=0 optionalFail=0 start=1793294413026 end=1793294593027
MICRO-FAIL events=0 uniquePaths=0
```

- The timestamp delta is **180001ms**, not 30s after HUD. The `duration=30s` scope label is therefore a nominal label, not the actual FINAL window.
- After FINAL, the log still shows optional retry/recovered events, for example:
  - `11862.webp` retry attempt=2, attempt=3, then recovered/OK
  - `11192.webp` retry attempt=2, then recovered/OK
  - `11081.webp` retry attempt=2, then recovered/OK
  - additional optional sprite OK lines after FINAL
- No `MICRO-FAIL` event appears after FINAL; there are 0 failure events in the entire raw log. Thus `fail=0` remains consistent as a failure-event count, but FINAL `ok/bytes/uniquePaths` does not include about 100 post-FINAL optional successes/retries. It is a snapshot, not a complete final-state total.

Impact: a 180s timeout snapshot can pass required resources while still missing a true HUD+30s window and post-FINAL optional completion. This is a timing/counter completeness CONCERN, not a required-resource failure.

## 5. 6020 / scene boundary

Screenshot shows an actual scene (咸阳, 烈火教父·神农·26级, NPC/UI visible). Raw log:

```text
SCENE-OK=0 05/0c lines=0
GAME-OK 6020 cmd=0a01 len=18 encrypted=des
```

`SCENE-OK`/`05/0c` is not instrumented; the scene is proven by the screenshot, not fabricated.

## 6. Release / hygiene

- Release APK SHA-256: `944a38e65a03867563c788cae13819652d082a6636c206fa1ac451936154428d`
- Release zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- `view51.js` SHA `6fc9b01d...`, `client.js` SHA `04b5ed96...`: unchanged
- Raw log has no password/token content; the only ticket reference is the metadata length form (`ticket=260B`).

## 7. Findings summary

| Item | Result |
|---|---|
| 87/87 required status=ok | PASS |
| absent=0 / requiredFail=0 | PASS |
| 16786117 60548 / 11_13 attempts3 recovered | PASS |
| 8277 final OK / 11199 final OK | PASS |
| visible error toast | PASS: no toast in screenshot |
| FINAL fail=0 | PASS as snapshot |
| 180001ms vs HUD+30s | CONCERN: not HUD+30s |
| post-FINAL optional retry/OK | CONCERN: FINAL counters omit later successes |
| UI actual scene | PASS |
| SCENE-OK / 05/0c | NOT OBSERVED; not fabricated |
| release p12 | PASS: 0 |
| view51/client/protocol | PASS: unchanged |

## 8. Boundary

This is local WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, whole-game acceptance, or HUD+30s timing. No product source, tests, Java or JS was modified; no push was performed.
