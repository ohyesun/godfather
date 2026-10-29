# QA Phase 6 T90 Final Signoff — HUD+30s

- Audit date: 2026-10-30
- HEAD: `f1a2f94` (pushed)
- Raw log: `android-host-smoke/app/src/main/cpp/PHASE6-COLD-RUN-T88.log` (1240 lines, SHA-256 `78649abae652f8dd4a6de96cf00179b488c8ab5da39cbe5c01a8232b9c4a22b2`)
- Screenshot: `PHASE6-UI-T88.png` (SHA-256 `02d1ad41ba94eec3fcb0c14e11f0e8418c98cb8b9cc3e0d27478db69f656a8f6`)
- Mode: raw-log/source/artifact review; no product/tests/Java/JS changes; no push.

## 1. Verdict

**PASS with CONCERN — functional scene entry PASS; only SCENE-OK instrumentation CONCERN.**

All required T90 gates are satisfied from the raw log and screenshot. `MICRO-HUD-READY` and FINAL are exactly 30s apart, 87/87 required paths are `status=ok`, final required/optional failures are zero, 8277 and 11199 are finally OK, and the screenshot shows the actual scene without a visible failure toast. The only remaining concern is that `SCENE-OK`/`05/0c` is not instrumented (`SCENE-OK=0`, `05/0c lines=0`); the scene is proven by the screenshot, not fabricated as a packet.

## 2. HUD+30s timing

Raw timestamps:

```text
10-30 01:33:27.405  ZG335MICRO: MICRO-HUD-READY window=HUD+30s
10-30 01:33:57.406  ZG335MICRO: MICRO-RUN-FINAL ... window=HUD+30s
```

Delta:

```text
01:33:57.406 - 01:33:27.405 = 30.001s
```

This is the required HUD+30s window, not the previous 180s process timeout. The `MICRO-SCOPE` line is present both at HUD-ready and FINAL with `paths=87 sha256=4a3a5015`.

## 3. Required paths and final counters

Raw FINAL:

```text
MICRO-RUN-FINAL ok=1015 bytes=41880591 uniquePaths=998 failureEvents=0 requiredFailFinal=0 optionalFailFinal=0 window=HUD+30s start=1793295012167 end=1793295237406
MICRO-FAIL events=0 uniquePaths=0
MICRO-FAIL-REQUIRED: <empty>
MICRO-FAIL-OPTIONAL: <empty>
```

Raw `MICRO-REQ` parsing:

```text
MICRO-REQ: 87
status=ok: 87
status=absent: 0
requiredFailFinal=0
```

Representative required checks:

- `native107/textures/16786117.png`: `MICRO-OK 60548` + `MICRO-REQ status=ok attempts=1`
- `scenes/xianyangcheng/11_13.bin`: `MICRO-OK 2205` + `MICRO-REQ status=ok attempts=1`
- `scenes/xianyangcheng/12_14.bin`, `13_13.bin`, and all other scope bins/map.json: `status=ok`

No required failure line exists.

## 4. 8277 / 11199 and toast

- `scenes/sprites/8277.webp`: `MICRO-OK path=... bytes=10399`; not in final optional fail.
- `scenes/sprites/11199.webp`: `MICRO-OK path=... bytes=21106`; not in final optional fail.
- `optionalFailFinal=0`.
- Screenshot `PHASE6-UI-T88.png` shows the actual scene and no visible “场景贴图读取失败” toast.

Final optional state is clean for these paths.

## 5. Scene screenshot and SCENE-OK boundary

Screenshot content:

- location `咸阳`
- player `烈火教父 · 神农 · 26级`
- minimap/actor/scene UI visible
- no visible error toast

Raw log:

```text
SCENE-OK=0 05/0c lines=0
GAME-OK 6020 cmd=0a01 len=18 encrypted=des
```

The scene is proven by the screenshot. `SCENE-OK`/`05/0c` was not emitted; this is an observer instrumentation CONCERN, not a fake packet or a scene failure.

## 6. Release / hygiene

- Release APK SHA-256: `1672ecfbd98e40f21fbcdddd495c4e8c746f34bc82538b1a1c71257520e35911`
- Release zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- `view51.js` SHA `6fc9b01d...`, `client.js` SHA `04b5ed96...`: unchanged
- Raw log has no password/token content; the only ticket reference is the metadata length form (`ticket=260B`).

## 7. Findings summary

| Item | Result |
|---|---|
| HUD-READY → FINAL = 30.001s | PASS |
| FINAL window=HUD+30s | PASS |
| 87/87 required status=ok | PASS |
| absent=0 / requiredFailFinal=0 | PASS |
| MICRO-FAIL-REQUIRED empty | PASS |
| 8277 final 10399 not optional fail | PASS |
| 11199 final 21106 | PASS |
| optionalFailFinal=0 | PASS |
| UI actual scene / no visible toast | PASS |
| MICRO-FAIL events=0 | PASS |
| SCENE-OK / 05/0c | CONCERN: not instrumented, not fabricated |
| release p12 | PASS: 0 |
| view51/client/protocol | PASS: unchanged |

## 8. Final disposition

**PASS with CONCERN (SCENE-OK instrumentation only).**

This signs the functional HUD+30s scene result and resource final-state semantics. It does not claim a `SCENE-OK` or `05/0c` packet and does not change the observer limitation. No product source, tests, Java or JS was modified; no push was performed.
