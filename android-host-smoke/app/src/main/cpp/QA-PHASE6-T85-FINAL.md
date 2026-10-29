# QA Phase 6 T85 Final Adjudication

- Audit date: 2026-10-30
- HEAD: `a79463c` (pushed)
- Raw log: `android-host-smoke/app/src/main/cpp/PHASE6-COLD-RUN-T85.log` (1225 lines, SHA-256 `7d4d16ee17c7006403ec5e4583021e42367b08a5efd339192bf8807af1e02b83`)
- Screenshot: `PHASE6-UI-T85.png` (SHA-256 `73a06d0f7a66974152800dd2090046ed72c3234fba9f6c2a89eaf24dcbc0d2c4`)
- Mode: raw-log/source/artifact review; no product/tests/Java/JS changes; no push.

## 1. Verdict

**CONCERNS.**

The 87/87 required paths are genuinely present and `status=ok`; `requiredFail=0` is supported for those 87 paths. The scene screenshot is real and has no visible error toast. However, `8277.webp` has a fail-then-OK event sequence while FINAL still reports `optionalFail=1`, and FINAL was emitted at a 180001ms process timeout rather than HUD+30s. These are metric/timing defects, not required-resource failures, so the result is CONCERNS rather than FAIL.

## 2. 87/87 required evidence

Raw `MICRO-REQ` parsing:

```text
scope paths: 87
MICRO-REQ: 87
status=ok: 87
status absent: 0
requiredFail: 0
MICRO-FAIL-REQUIRED: absent (no line)
```

`MICRO-SCOPE`:

```text
MICRO-SCOPE map=xianyangcheng spawn=138,184 duration=30s paths=87 sha256=4a3a5015
```

Per-path checks:

- `scenes/xianyangcheng/12_14.bin`: `MICRO-REQ ... status=ok attempts=1`
- `scenes/xianyangcheng/13_13.bin`: `MICRO-REQ ... status=ok attempts=1`
- `scenes/xianyangcheng/11_13.bin`: `MICRO-REQ ... status=ok attempts=1` (no retry)
- `native107/textures/16786117.png`: `MICRO-REQ ... status=ok attempts=1` and `MICRO-OK 60548`
- `native107/textures/16777577.png`: present in the 87 scope and `status=ok`
- `native107/textures/16786122.png`: present in the 87 scope and `status=ok`

Thus the 87/87 required claim is supported by the raw log.

## 3. 8277 fail-then-OK and FINAL count

Raw sequence:

```text
MICRO-RETRY path=scenes/sprites/8277.webp class=optional attempt=1
MICRO-FAIL path=scenes/sprites/8277.webp code=io
MICRO-OK path=scenes/sprites/8277.webp bytes=10399
...
MICRO-FAIL-OPTIONAL scenes/sprites/8277.webp
```

`MicroStats.recordFail()` adds the path to `FAIL_PATHS` and increments `fail`; `recordOk()` updates success bytes and `REQUESTS` status but does **not** remove the path from `FAIL_PATHS` or decrement `fail`. Therefore:

- `8277` final asset state is **OK (10399B)**;
- FINAL `optionalFail=1` is a **stale failure-event count**, not a final missing-asset count;
- the report cannot simultaneously use `8277=OK` and present `optionalFail=1` as final failure. The counter semantics are wrong/misleading.

This is a **CONCERNS** metric defect; it is not a required-resource failure. A correct FINAL should either remove recovered paths from final failure sets or explicitly name the metric `failureEvents`, not `optionalFail`.

## 4. 30s vs 180s window

Raw FINAL timestamps:

```text
start=1793293775726
end=1793293955727
delta=180001ms
```

`MICRO-SCOPE` labels `duration=30s`, but there is no `sceneReady`-driven 30s window; FINAL was emitted at the **180s timeout**. The separate 30s cold attempt was blocked before HUD (screenshot showed loading/retry), so 30s-after-HUD was not achieved. This is **not acceptable as a 30s-after-HUD result**; it is a 180s bounded-run result. The report should label it accordingly or a new HUD-ready rerun is required. The screenshot still proves the scene visually.

## 5. Optional toast and 11199

- `11199.webp`: `MICRO-OK 21106`; not in failure list.
- `8277.webp`: failure event followed by `MICRO-OK 10399`; screenshot has no visible `场景贴图读取失败` toast.
- Optional `8277` is allowed to be non-fatal, but the stale final `optionalFail=1` plus the earlier failure event means a user-visible toast risk remains. This is CONCERNS, not a clean PASS.
- No optional path is rewritten to zero.

## 6. Scene / 6020 boundary

Screenshot `PHASE6-UI-T85.png` shows an actual loaded scene:

- 咸阳
- 烈火教父 · 神农 · 26级
- minimap and scene HUD/NPCs visible
- no visible error toast

Raw log has:

```text
SCENE-OK=0 05/0c lines=0
GAME-OK 6020 cmd=0a01 len=18 encrypted=des
```

`SCENE-OK`/`05/0c` is not instrumented; the scene is proven by the screenshot only, not by a fabricated packet log.

## 7. Release / hygiene

- Release APK SHA-256: `03971bcf957d93cf7ea09d2cba34c26530d016e39436f221c184b64e33c06bb7`
- Release zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- `view51.js` SHA `6fc9b01d...`, `client.js` SHA `04b5ed96...`: unchanged.
- Raw log contains no password/token content; the only ticket reference is the existing metadata length form (`ticket=260B`).

## 8. Findings summary

| Item | Result |
|---|---|
| 87/87 required status=ok | PASS |
| absent=0 / requiredFail=0 | PASS |
| 16786117 / 11_13 attempts=1 ok | PASS |
| 11199=21106 | PASS |
| 8277 fail-then-OK | PASS as final asset state; CONCERN in FINAL counter |
| FINAL optionalFail=1 after 8277 OK | CONCERN: stale failure-event count |
| FINAL 180001ms vs HUD+30s | CONCERN: not a 30s-after-HUD window |
| No visible error toast | PASS for screenshot; optional toast risk remains |
| UI actual scene | PASS |
| SCENE-OK / 05/0c | NOT OBSERVED; not fabricated |
| release p12 | PASS: 0 |
| view51/client/protocol | PASS: unchanged |

## 9. Boundary / T83

T83's all-sprite retry is not signed by this report. This is a final adjudication of T85's raw evidence only. A corrected FINAL should distinguish failure events from final asset state and use the actual HUD-ready 30s window (or explicitly relabel the 180s bound).

No product source, tests, Java or JS was modified; no push was performed.
