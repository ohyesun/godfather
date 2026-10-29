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

## Not yet connected

`PackedAssetsPathHandler` still has the local-only negative branch. UI resource
fallback (PakRemote equivalent) is the next milestone; the current map-loading
state therefore still must be split into:

- 6020 scene-frame progress
- 18008 micro-resource fetch/fallback

No token/resource content is logged.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,836,490 | `c12e5d1e51c89b97317180f42dc3e2cc910041b040dabd580ef01da072000615` |
| `app-release-unsigned.apk` | 54,782,690 | `ffdfabbe4e2aadc2eb56a167180f4860a383f03396968ea01bc4d713f17d1ebd` |

Release keystore scan: 0 hits for `keystore|zg_test|.p12|.jks`; release remains
unsigned.

## Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `Zgmu.java` | 8395 | `39d15fd642a21102e421dd897af511b17768a6bdff7edf74f5a989b352f70882` |
| `MicroProbe.java` | 4204 | `d4b2755dd6c7a0427f8a5f17149641ddc1dacb9439a6267e09a8b476ceedce31` |
| `MainActivity.java` | 15027 | `483093d502479fd76f824d41efcf8591c0bc3a7d566a45478a7d4b85ce7fbd1c` |

No token/password content printed; no view51/client.js/protocol changes.
