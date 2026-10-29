# Host Direct 7000 Login Phase 4 Report

Task: T72. New host WebView now exposes a minimal `window.Lab` raw-TCP bridge
and completes real 7000 login to the selected PC zone. No view51/client.js/
protocol/old Java/assets changes.

## Implementation

New classes:

- `DirectTcpBridge.java`
  - `window.Lab.request95(id, endpoint, route, credential, account, body)`
  - `window.Lab.request(...)` compatibility alias
  - `window.Lab.report(...)` no-op (the page polls diagnostics)
- `Login7000Monitor.java`
  - passive RC5-32/12/16 decrypt of the 7000 receive stream
  - parses 68/04 and logs only metadata

`MainActivity` adds `Lab` as a JavaScript interface and closes the direct
socket on destroy.

## Routes

| Route | Status | Notes |
|---|---|---|
| `/direct/zones` | supported | returns `configText` with four PC zones |
| `/direct/open` | supported | raw TCP connect, 6s connect timeout |
| `/direct/send` | supported | packetHex → socket write |
| `/direct/receive` | supported | 25ms read timeout, returns dataHex |
| `/direct/close` | supported | closes socket |
| `/direct/register` | not implemented | out of this login-only phase |
| `/mobile/*` | not implemented | this phase only needs direct login |

Zone list returned to the page:

- 新开四区 `45.124.78.8:7000`
- 复古三区 `45.124.76.59:7000`
- 复古二区 `45.124.76.73:7000`
- 复古一区 `45.124.78.3:7000`

The page already contains the matching zone buttons and login form. Account and
password are read only from those UI inputs. No credentials are logged or
stored in source. A temporary local extraction used for the device test was
deleted after the run.

## Real device result

MuMu, WebView 110, selected 新开四区:

```text
I ZG335LOGIN: LOGIN-OK accid=1 game=45.124.78.8:6020 ticket=260B
```

The UI reached the character roster and displayed:

- 烈火教父 26级
- 战国七雄 1级

The first attempt selected 复古一区 `45.124.78.3:7000` and failed at TCP
connect with `SocketTimeoutException`; switching to 新开四区 succeeded. This
was a zone-selection issue, not a crypto/68/10/68/04 failure. No fake key or
account fallback was used.

A separate observation: while the old APK `com.zhanguo.chaptertwo` still held
the account/session, the host's first `45.124.78.8:6020` open timed out at
6000ms. After force-stopping the old APK, the retry reached the role list.
This is session/port contention, not RC5 or 68/04 failure; QA records it as a
CONCERN, not a PASS condition.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,828,860 | `f4bc2b42fa69c57c1ad6dd3c3c544c17512d9116687a8de0aaec0631b70cfd91` |
| `app-release-unsigned.apk` | 54,778,594 | `432142ea70c8a7245726458d2fddcd7bc9c1b7538502548887527b50db0419eb` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `app/src/main/java/com/zg335/host/DirectTcpBridge.java` | 8096 | `ad216a6911d732ef5cbdcbe44ed52e36cba33600f03cdff0839f7663147bc42c` |
| `app/src/main/java/com/zg335/host/Login7000Monitor.java` | 6312 | `4a510dddc01e9426555bcd561ea286cd091eb2d05d7f26db68d7a27f31102c0f` |
| `app/src/main/java/com/zg335/host/MainActivity.java` | 14857 | `bf57e5c3a4c926ec1ce3c3d513a07148d7ef155c1306259b8331d7edc7359211` |

No view51/client.js/protocol/record-layer/old Java/assets changes. This commit
was pushed as `ef7500a`; the earlier "no push" line was a stale template line.
