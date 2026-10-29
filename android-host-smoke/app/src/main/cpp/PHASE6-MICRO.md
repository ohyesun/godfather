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

## Not yet connected

UI login/selection rerun and the separation of `05/0c` / `SCENE-OK` from
`MICRO-OK` counts is not yet completed.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,839,725 | `cb6609845b6f55958fdecabae460438f2465c5ffe3cca35ddce503b63066376f` |
| `app-release-unsigned.apk` | 54,786,786 | `e5cfef6b63831e2a53364b935a1e9a83baeb9a5ad8912cc2b511b47ec2132af6` |

Release keystore scan: 0 hits for `keystore|zg_test|.p12|.jks`; release remains
unsigned.

## Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `Zgmu.java` | 8395 | `39d15fd642a21102e421dd897af511b17768a6bdff7edf74f5a989b352f70882` |
| `MicroProbe.java` | 4204 | `d4b2755dd6c7a0427f8a5f17149641ddc1dacb9439a6267e09a8b476ceedce31` |
| `PakRemote.java` | 7013 | `52a8d77bb27851a0986836aec86484e4424e2dcc18956659f36d82e5c81858e3` |
| `PackedAssetsPathHandler.java` | 9315 | `3334340dad142242cdb2d9d741053f6454558cb31d7739d8a31f9252aa29e3a1` |
| `MainActivity.java` | 15027 | `483093d502479fd76f824d41efcf8591c0bc3a7d566a45478a7d4b85ce7fbd1c` |

No token/password content printed; no view51/client.js/protocol changes.
