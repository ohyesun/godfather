# 6020 Session First-Frame Phase 5 Report

Task: T75. Host-owned login was extended to observe the 6020 session and the
first DES-encrypted server frame. No view51/client.js/protocol/old Java/assets
changes.

## Setup

- Old APK `com.zhanguo.chaptertwo` force-stopped before the run:
  `pidof` returned empty.
- Host selected 新开四区 `45.124.78.8:7000` from its own UI.
- 7000 login reached the roster: 烈火教父 26级 and 战国七雄 1级.
- `Login7000Monitor` retained the 68/04 DES key in memory only.

## 6020 result

```text
I ZG335LOGIN: LOGIN-OK accid=1 game=45.124.78.8:6020 ticket=260B
I ZG335GAME: GAME-OK 6020 cmd=0a01 len=18 encrypted=des
```

The first valid DES frame was decrypted with the key extracted from 68/04.
The log contains no ticket, key, account or password bytes.

## Character / scene boundary

The host opened the role screen and 烈火教父 was selected; “进入游戏” was
clicked. The UI then remained at “正在载入地图… / 仍在等待游戏数据或资源”.
No further scene/map proof was claimed. `sceneQA`/objects were not asserted as
live game state. This phase proves:

- 7000 login and roster worked;
- 6020 TCP connect worked;
- the 6020 DES key was valid and the first frame decrypted.

It does **not** prove full map entry. The next failure point is the character
select/scene phase after the first 6020 frame, not TCP/RC5/DES-key parsing. No
fake key or account retry was used.

## APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,830,144 | `683520c3cceb892f1e5b19d356913e3d39d73e6dbab5a5ee01a96e2c2b1173c4` |
| `app-release-unsigned.apk` | 54,778,594 | `25005d13ec5a808f4bc701feba8e2987632c229371876219f9d04f36c9da3f66` |

Keystore scan:

- debug: `res/raw/zg_test_keystore.p12` (expected)
- release: **0 hits** for `keystore|zg_test|.p12|.jks`
- release remains unsigned.

## Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `DirectTcpBridge.java` | 8678 | `36283a1695df2918a46d1ce65d5d0584ae840bf2b9ba7d25dab6c33321d16049` |
| `Login7000Monitor.java` | 6714 | `0a8db716b4fe9743ac558e07dc6757937b66ea5930d712fe3eeff354a4e4cb37` |
| `Game6020Monitor.java` | 2333 | `dc74b59d7b7a63888c1b4e072e34597d2ac7e616be5c2f765dde69b6d03679d4` |
| `MainActivity.java` | 14857 | `bf57e5c3a4c926ec1ce3c3d513a07148d7ef155c1306259b8331d7edc7359211` |

No pcap/account/ticket content, view51/client.js/protocol/record-layer/old
Java/assets changes. The report commit is pushed immediately after this file is
written.
