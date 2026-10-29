# 6020 Session First-Frame Phase 5 Report

Task: T75 (+ bounded retry decision). Host-owned login was extended to observe
the 6020 session and the first DES-encrypted server frame. No
view51/client.js/protocol/old Java/assets changes.

## Setup

- Old APK `com.zhanguo.chaptertwo` force-stopped before the run; `pidof`
  returned empty.
- Host selected 新开四区 `45.124.78.8:7000` from its own UI.
- 7000 login reached the roster: 烈火教父 26级 and 战国七雄 1级.
- `Login7000Monitor` retained the 68/04 DES key in memory only.

## 6020 result

```text
I ZG335LOGIN: LOGIN-OK accid=1 game=45.124.78.8:6020 ticket=260B
I ZG335GAME: GAME-OK 6020 cmd=0a01 len=18 encrypted=des
```

- First valid DES frame: `cmd=0a01`, `len=18`.
- This run used the **first 6020 connection successfully; no retry was
  triggered**.
- Logs contain no ticket/key/account/password bytes.

## Bounded retry rule

Only the 6020 connect phase has a single bounded retry:

- first `SocketTimeoutException` → wait 1s → one retry
- log: `GAME-RETRY 6020 attempt=2 reason=timeout`
- second timeout → log
  `GAME-FAIL 6020 attempt=2 reason=timeout possible-old-client-session`
  and fail; no further retries
- receive polling (`/direct/receive`) is not retried as a connection attempt
- timeout is never treated as success; ticket/DES is never skipped

No retry occurred in this successful run, so no `GAME-RETRY`/`GAME-FAIL` line
was emitted.

## Character / scene boundary

The host opened the role screen and 烈火教父 was selected; “进入游戏” was
clicked. The UI then remained at “正在载入地图… / 仍在等待游戏数据或资源”.
No further scene/map proof was claimed. This phase proves:

- 7000 login and roster worked;
- 6020 TCP connect worked;
- the 6020 DES key was valid and the first frame decrypted.

It does **not** prove full map entry. The remaining failure point is the
character select/scene phase after the first 6020 frame, not TCP/RC5/DES-key
parsing. No fake key or multiple retry was used.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,830,434 | `83181fa2dd696c45528407ee45105db258e00a741a68ff24e5175881c973aca7` |
| `app-release-unsigned.apk` | 54,778,594 | `38591f7aefba6d815f283bd01341afcab1c64e38f6a640608fbc16472ea3d27d` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `DirectTcpBridge.java` | 9921 | `0dc8a1a63b3020a4958365e8dc0c6e1504bd17fac63b195e95b6d9444f501779` |
| `Login7000Monitor.java` | 6714 | `0a8db716b4fe9743ac558e07dc6757937b66ea5930d712fe3eeff354a4e4cb37` |
| `Game6020Monitor.java` | 2333 | `dc74b59d7b7a63888c1b4e072e34597d2ac7e616be5c2f765dde69b6d03679d4` |

No pcap/account/ticket content, view51/client.js/protocol/record-layer/old
Java/assets changes; no push.
