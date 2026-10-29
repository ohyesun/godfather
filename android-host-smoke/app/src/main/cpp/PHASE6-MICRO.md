# ZGMU/18008 First Milestone (T77)

## Audit result

The host had no ZGMU/18008 implementation:

- no `Zgmu`/`PakRemote`/`PakFetcher` Java classes
- `PackedAssetsPathHandler` only served AssetManager or local `resources107.pak`
- negative-offset/length entries logged `pak miss negative` and returned `null`
- `resources107-index.json`: 82,344 entries, **80,457 negative/remote**, 1,887 local
- runtime `/proc/net/tcp`/`tcp6` had no remote `:4658` (18008)

The `micro` counter in `client.js` is a protocol codec empty-frame counter, not
a micro-resource request.

## First milestone: one real `path:` fetch

Added:

- `Zgmu.java`: ZGMU/1 HELLO (0x01/0x81), GET (0x10/0x90), END flag 0x02,
  `path:<path>`, offset=-1 remote sentinel. HELLO_ACK `manifestSize` is read at
  **offset 15**, not 7.
- `MicroProbe.java`: debug-only probe reading config from `micro-client.json`;
  no token is hardcoded or logged.
- `MainActivity`: debug builds run the probe on a background thread; no UI
  route is connected yet.

Selected remote path:

```text
native107/textures-report.json
index=[-1, 40, 0]
stored length=40, flag=0, expected SHA-256 present
```

Device log (MuMu):

```text
I ZG335MICRO: MICRO-OK path=native107/textures-report.json bytes=40
```

This proves HELLO/GET/DATA/END and the `path:` + offset=-1 path work against
the real 18008 server. The probe validates exact length and SHA-256 before
emitting `MICRO-OK`.

## Second milestone: negative-offset handler fallback

`PakRemote` now implements the negative-offset path:

1. local cache hit (`filesDir/pakfetch/<sha256("path:"+name)>.bin`)
2. otherwise ZGMU `GET path:<name>` and cache
3. return gzip stream when index flag=1
4. refuse remote `.js/.mjs/.html/.htm/.css/.svg`

`PackedAssetsPathHandler` now calls this layer for `offset<0 || length<0`
instead of returning `null`; failures fall back to a null response and are
logged, not thrown.

Real device logs from the handler path:

```text
MICRO-OK path=native107/textures-report.json bytes=40 source=pak-remote
MICRO-OK path=native107/model-aliases107.json bytes=1650
MICRO-OK path=native107/electricity107.json bytes=806
MICRO-OK path=native107/bones.json bytes=20287
MICRO-OK path=scenes/xianyangcheng/map.json bytes=2709
MICRO-OK path=scenes/xianyangcheng/10_11.bin bytes=2165
MICRO-OK path=scenes/xianyangcheng/12_11.bin bytes=2264
...
MICRO-FAIL code=io
```

The negative offset path is therefore reaching 18008 and caching actual
`scenes/xianyangcheng/*` resources. One `MICRO-FAIL code=io` occurred among
otherwise successful loads; it is logged and does not crash the handler.

## UI rerun: new-four zone

Device: MuMu `emulator-5554`; old APK `com.zhanguo.chaptertwo` force-stopped.
Selected **新开四区** (45.124.78.8:7000), logged in, selected 烈火教父, clicked
“进入游戏”.

Result: **entered the actual game scene** (咸阳城, character 烈火教父 26级,
NPCs/minimap visible). This is not just a startup resource probe.

Resource layer:

```text
MICRO-OK count = 800
MICRO-OK total bytes = 22,530,003
MICRO-FAIL = 0
```

The loaded set includes `scenes/xianyangcheng/map.json` and multiple
`scenes/xianyangcheng/*.bin` files.

6020 layer progressed through the select/tail/heartbeat sequence (logs show
`GAME-TAIL order=0452,0452,0a03,0452` and continued `0452/0a03`). The 6020
observer still only logs the first frame as `GAME-OK 0a01`; it does not emit
a separate `SCENE-OK` line. Therefore “UI entered game” is proven visually,
while `05/0c` / `SCENE-OK` is **not separately instrumented in this run**; the
two layers are not conflated.

`MICRO-FAIL` now includes the failing `path=` for future diagnosis.

## T78 single-run reconciliation

Added process-level `MICRO-RUN` counters; automatic startup `MicroProbe` was
removed so the UI run is not polluted by a separate probe. Before the run:
force-stop both APKs, `pm clear com.zg335.host`, `logcat -c`. Then one
新开四区 login/select/enter run.

Counter line:

```text
MICRO-RUN ok=797 bytes=24795764 uniquePaths=779 fail=33 start=1793287221571 end=1793287397380
```

Comparison:

```text
author:  ok=800  bytes=22530003  (no unique counter)
QA:      ok=812  bytes=30752347  uniquePaths=793
T78 run: ok=797  bytes=24795764  uniquePaths=779 fail=33
```

The QA 812/30.7 MB includes multiple process starts / earlier probe and is not
one run. The single run is 797, not 800; QA CONCERN therefore remains open.
33 failures cover 31 unique failing paths (below), so 779 unique successes +
31 unique failures = 810 potential unique paths; retries/failure paths explain
the unique gap rather than a single-run 800.

31 unique failing paths:

```text
native107/textures/16777577.png
native107/textures/16786122.png
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
scenes/xianyangcheng/12_14.bin
scenes/xianyangcheng/13_13.bin
```

Despite those failures, the UI entered the actual game scene (咸阳/听泉寨,
烈火教父 26级); screenshot `PHASE6-UI-T78.png`. The 6020 observer still only
prints the first `0a01`; no separate `05/0c`/`SCENE-OK` log was produced, so
the scene claim is from the UI screenshot, not a fabricated log.

## T80 bounded retry on required paths

Required paths: `scenes/xianyangcheng/12_14.bin`, `13_13.bin`,
`native107/textures/16777577.png`, `16786122.png`, and `scenes/**/*.bin` /
`scenes/**/map.json`. Optional `scenes/sprites/*.webp` are not retried.

Retry policy: only transient `io` / server E_IO, at most 2 retries, delays
100/300ms. No retry for `E_NO_PACK`/`E_OFFSET`/`E_AUTH`/`E_PROTO`. Final
failure is logged with path and code. Fetch concurrency is capped at 12; OK/
bytes are counted only after a successful fetch.

Clean single-run result:

```text
MICRO-RUN ok=796 bytes=24810993 uniquePaths=777 fail=24
MICRO-RETRY path=scenes/xianyangcheng/11_13.bin attempt=2 reason=io
```

Required-path failures: **0**. The only retry was the required map block
`11_13.bin`; it succeeded on attempt 2. The 24 final failures are optional
`scenes/sprites/*.webp`, left visible without fabricating assets. UI still
entered the real scene; screenshot `PHASE6-UI-T80.png`.

This is not a claim that the total is 800: the single-run total is 796/24.8 MB,
with required resources complete. The optional-sprite failures remain.

## T80 same-run three-group record

One warm-cache run after force-stop + `logcat -c` (same process, no separate
probe), 新开四区 → 烈火教父 → enter:

```text
MICRO-RUN ok=797 bytes=24802468 uniquePaths=780 fail=0
MICRO-FAIL events=0 uniquePaths=0
SCENE-OK=0 05/0c lines=0
GAME-OK 6020 cmd=0a01 len=18 encrypted=des
```

The UI screenshot `PHASE6-UI-T80-RUN.png` shows the actual game scene
(咸阳/听泉寨, 烈火教父 26级). The same run's required-resource failure list is
empty. `SCENE-OK` is 0 because the passive 6020 observer did not parse a
`05/0c` frame in this build; the scene is proven by the UI screenshot, not by
that counter.

**Statistics-window clarification:** T79's `33 events / 31 paths` came from
the cold-cache single run; T78 QA's `MICRO-FAIL=0` and this T80 warm run's
`events=0` are different statistics windows (cold vs warm cache). They are not
the same run and must not be compared as one `MICRO-FAIL=0` conclusion.

## T80 final frozen counter

Classification fixed before this run:

- all `native107/textures/*.png` are required
- all `native107/*.glb` are required
- `scenes/**/*.bin` and `scenes/**/map.json` are required
- only `scenes/sprites/*.webp` remain optional

`native107/textures/16786117.png` was probed directly against 18008:
`index=[-1,60548,0]`, `OK 60548/60548`; it is therefore required, not a
skipped optional sprite.

Single clean run FINAL (once, frozen):

```text
MICRO-RUN-FINAL ok=1159 bytes=51246524 uniquePaths=1158 fail=6
MICRO-FAIL events=6 uniquePaths=6
MICRO-FAIL-REQUIRED: <empty>
MICRO-FAIL-OPTIONAL scenes/sprites/7474.webp,11369.webp,7468.webp,8467.webp,8683.webp,13325.webp
SCENE-OK=0 05/0c lines=0
```

Required failures: **none**. The six final failures are all optional sprites.
The UI screenshot `PHASE6-UI-T80-FINAL.png` shows the actual scene
(咸阳/听泉寨, 烈火教父 26级). `SCENE-OK=0` is the passive observer's missing
05/0c instrumentation, not a claim that the UI failed; the screenshot is the
scene evidence.

Statistics window: this FINAL is one process/one run. T79's 33 events/31 paths
was the earlier cold-cache run; T78 QA's `MICRO-FAIL=0` was another window.
No `MICRO-FAIL=0` is reused as a cross-window conclusion.

## T82 cold raw run

Clean cold run (force-stop, `pm clear com.zg335.host`, `logcat -c`, one
新开四区 login/select/enter). The sanitized raw log is committed at
`app/src/main/cpp/PHASE6-COLD-RUN.log` (MICRO/GAME/LOGIN lines; GAME-RAW and
GAME-PLAIN removed; no token/password/content).

```text
MICRO-RUN-FINAL ok=1054 bytes=43539864 uniquePaths=1044 fail=37 requiredFail=0 optionalFail=36
MICRO-FAIL events=37 uniquePaths=36
MICRO-FAIL-REQUIRED: <empty>
MICRO-FAIL-OPTIONAL: 36 scenes/sprites/*.webp paths (full list in PHASE6-COLD-RUN.log)
SCENE-OK=0 05/0c lines=0
GAME-OK 6020 cmd=0a01 len=18 encrypted=des
```

- `native107/textures/16786117.png` was checked directly: index `[-1,60548,0]`,
  18008 `OK 60548/60548`; it is required and not in the optional list. It was
  not the failing path in this run.
- Required failures: **0**.
- UI screenshot `PHASE6-UI-COLD-FINAL.png`: actual game scene entered
  (咸阳/听泉寨, 烈火教父 26级).
- `SCENE-OK=0` is the observer's missing 05/0c instrumentation; the screenshot
  is the scene evidence, not a fabricated SCENE-OK.

`MICRO-FAIL=0` from T78 QA and T79's 33/31 are different windows; this cold
FINAL has 37 events/36 paths and must not be collapsed into either.

## Not yet connected

UI login/selection rerun and the separation of `05/0c` / `SCENE-OK` from
`MICRO-OK` counts is not yet completed.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,843,965 | `0b2a0e38f6bae7f6ed2f7edf9a073861e79c92852fbc6f3b17dffa73291c492b` |
| `app-release-unsigned.apk` | 54,790,882 | `16a88f63afd1dc170ab5f65260b8b287571ecd055bfe9d9958481e1cd1dc96bf` |

Release keystore scan: 0 hits for `keystore|zg_test|.p12|.jks`; release remains
unsigned.

## Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `Zgmu.java` | 8395 | `39d15fd642a21102e421dd897af511b17768a6bdff7edf74f5a989b352f70882` |
| `MicroProbe.java` | 4204 | `d4b2755dd6c7a0427f8a5f17149641ddc1dacb9439a6267e09a8b476ceedce31` |
| `PakRemote.java` | 10302 | `ad648167eeb2dc935c6b4d9547c51cb896e19135f96eb2d61e8d96cffb64b932` |
| `PackedAssetsPathHandler.java` | 9315 | `3334340dad142242cdb2d9d741053f6454558cb31d7739d8a31f9252aa29e3a1` |
| `MicroStats.java` | 2593 | `d6dd76e5d250d2fe1c6ce5bae406396ce7b7439eb94571458392503864f04e70` |
| `Game6020Monitor.java` | 3260 | `3d61df803ef4a7b690014d967b9ba5d7f44109f1c31ec683a2a9e708fbb03bdb` |
| `MainActivity.java` | 15027 | `483093d502479fd76f824d41efcf8591c0bc3a7d566a45478a7d4b85ce7fbd1c` |

No token/password content printed; no view51/client.js/protocol changes.
