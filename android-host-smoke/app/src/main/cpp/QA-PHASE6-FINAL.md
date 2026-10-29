# QA Phase 6 Final — T80 Cold/Warm Adjudication

- Audit date: 2026-10-28
- HEAD: `405836d` (pushed)
- Screenshot: `android-host-smoke/app/src/main/cpp/PHASE6-UI-T80-RUN.png` (SHA-256 `88a159cb9b3b5c6d1be56e558fcdaf3879096dee365203f3a6e7a8f45e5321fa`)
- Mode: read-only source/report/artifact review; no product/tests/Java/JS changes; no push.
- Cold rerun was not performed in this QA window. The committed T80 cold counters/triage were reviewed; limitations are called out explicitly.

## 1. Verdict

**CONCERNS.**

The T80 warm run is clean (`797`, `24,802,468` bytes, `fail=0`), but that is a different cache window from cold. The committed cold T80 report claims `24` optional failures and `0` required failures; however the raw cold log is not preserved, `16786117.png` is missing from the committed required-path evidence, and the exact 24 optional failure paths are not enumerated. UI scene entry is confirmed by the T80 screenshot; `SCENE-OK`/`05/0c` was not emitted and is not claimed.

## 2. Cold / warm separation

| Window | MICRO-RUN / evidence | Meaning |
|---|---|---|
| T80 warm | `ok=797 bytes=24,802,468 uniquePaths=780 fail=0`; `MICRO-FAIL events=0` | warm cache only; does **not** prove cold has no failures |
| T79 cold | `33` failure events / `31` unique paths | cold baseline failures |
| T80 cold | `ok=796 bytes=24,810,993 uniquePaths=777 fail=24`; required failures reported `0`; one retry `scenes/xianyangcheng/11_13.bin attempt=2` | cold bounded-retry run; 24 optional failures remain |

The warm `fail=0` must not be combined with the cold `33`/`24` evidence.

## 3. Required cold resources

The T80 report identifies these as required:

```text
scenes/xianyangcheng/12_14.bin
scenes/xianyangcheng/13_13.bin
scenes/xianyangcheng/11_13.bin   (retry attempt=2, succeeded)
native107/textures/16777577.png
native107/textures/16786122.png
```

`16786117.png` is **required by code policy**: `PakRemote.isRequired()` classifies every `native107/textures/` path as required, so it cannot be labeled optional without contradicting the implementation. The user-provided cold observation says it logged `code=io`; however the committed T80 report/triage does not contain a preserved cold log line or retry/success entry for this path. Therefore:

- classification: **required**, not optional;
- required-fail claim `0`: **not fully independently evidenced for 16786117**;
- no fake “0” is asserted here.

The T80 report says all required final failures were zero, and `11_13.bin` retried successfully. That claim is accepted only as report evidence for the listed paths, with the 16786117 evidence gap noted.

## 4. Optional sprite failures

T80 cold reports **24 optional `scenes/sprites/*.webp` failures**. These are allowed to fail, but must remain visible. The exact T80 24-path set was not committed. T79 triage provides the following 27 optional candidates, all recorded with `code=io`:

```text
scenes/sprites/12374.webp
scenes/sprites/12427.webp
scenes/sprites/13326.webp
scenes/sprites/13738.webp
scenes/sprites/14185.webp
scenes/sprites/14209.webp
scenes/sprites/14222.webp
scenes/sprites/14411.webp
scenes/sprites/14452.webp
scenes/sprites/14485.webp
scenes/sprites/14505.webp
scenes/sprites/14672.webp
scenes/sprites/4569.webp
scenes/sprites/7432.webp
scenes/sprites/7457.webp
scenes/sprites/7491.webp
scenes/sprites/7497.webp
scenes/sprites/8034.webp
scenes/sprites/8249.webp
scenes/sprites/8252.webp
scenes/sprites/8346.webp
scenes/sprites/8461.webp
scenes/sprites/8872.webp
scenes/sprites/9075.webp
scenes/sprites/9720.webp
scenes/sprites/9721.webp
scenes/sprites/9885.webp
```

The T80 final count is `24`, a subset of these 27 candidates; without the raw cold T80 log the exact four excluded paths cannot be proven. The 24 are **not** rewritten as 0.

## 5. Scene evidence and 6020 boundary

T80 screenshot shows an actual loaded scene:

- 咸阳
- 烈火教父 · 神农 · 26级
- minimap `186,395`
- visible 听泉寨 NPCs

This is not a login/loading page. The 6020 observer emitted only:

```text
GAME-OK 6020 cmd=0a01 len=18 encrypted=des
SCENE-OK=0
05/0c lines=0
```

Therefore the report states: **进图由 UI 截图确证，`SCENE-OK` 未打点；没有伪造 `05/0c` 或 `SCENE-OK`。**

## 6. Release / hygiene

- Current release APK SHA-256: `e803abbd63c33ecdd36e8adeaee4db585bcd34115d680bda049c2f05221a0917`
- Release zip scan `keystore|zg_test|.p12|.jks`: **0 matches**
- `view51.js` SHA `6fc9b01d...` and `client.js` SHA `04b5ed96...` remain unchanged.
- No app-tag token/password/resource-content log was found in the reviewed evidence; the application logs carry path/bytes/code metadata.

## 7. Findings summary

| Item | Result |
|---|---|
| Warm run 797 / 24,802,468 / fail0 | PASS as warm only |
| Cold/warm separation | PASS: not merged |
| Cold T80 required failures | CONCERN: report says0; 16786117 required evidence missing |
| 16786117 classification | CONCERN: required by code; no raw cold retry/success line |
| Optional sprites count 24, not 0 | PASS as count; exact 24 paths not preserved |
| Optional path/code list | CONCERN: 27 T79 candidates listed with io; T80 exact 24 unavailable |
| UI screenshot real scene | PASS |
| SCENE-OK / 05/0c | NOT OBSERVED; not fabricated |
| release p12 | PASS: 0 |
| view51/client/protocol unchanged | PASS |

## 8. Evidence limitation

This is an evidence review, not a new cold-cache device run. The current committed artifacts do not include the raw T80 cold log, the exact 24-path optional list, or a 16786117 retry/success line. A cold rerun with logcat capture is required to promote the exact required-fail claim to PASS.

## 9. Boundary

This is local WebView/APK QA evidence on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, whole-game acceptance, or `SCENE-OK`/`05/0c`. No product source, tests, Java or JS was modified; no push was performed.
