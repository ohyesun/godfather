# 6020 Handshake Byte-Level Diagnostic (T76)

Task: T76. This report compares the real pcap client→server 6020 blob with the
host's captured 384-byte TX blob. No view51/client.js/protocol/old Java/assets
changes.

## 1. pcap client→server 6020 (384B)

The first 72 bytes are **not an opaque ticket prefix**; they are RC5-encrypted
`68/78` + `68/05` frames. Decoding the first 72 bytes with the 6020 static RC5
key yields:

```text
68/78 body 20B
68/05 body 80B (RC5/zlib login credential frame)
```

The 72-byte prefix ciphertext:

```text
83d204cfa843d8db0df734d88df32d60147302911860fcef9c78b1ba9e3e58a9
cb16311c0d8548b6e06eeae09285fac6a8ca6e0bca06b3e2960f507e0fcf4e40
b5af49e171cb0c1f
```

Full pcap 384B ciphertext:

```text
83d204cfa843d8db0df734d88df32d60147302911860fcef9c78b1ba9e3e58a9
cb16311c0d8548b6e06eeae09285fac6a8ca6e0bca06b3e2960f507e0fcf4e40
b5af49e171cb0c1ffbab899ee8d21998c364f73594173b5878438d2296265e05
c5c84e7d9afbbd99a2a5e4e6da123c1078438d2296265e05c5c84e7d9afbbd9
9a2a5e4e6da123c1078438d2296265e05dfce8e13010b603478438d2296265e05
78438d2296265e05f41710a216a68e8837e61a345b8a2aba78438d2296265e05
78438d2296265e05172d2e69edd0ca6025f61894ccf751c478438d2296265e05
df2f7d91cc6becf425f61894ccf751c478438d2296265e05876324467e791049
78438d2296265e0539e800f56949865878438d2296265e0578438d2296265e05
78438d2296265e054f6e390fed4056a537e61a345b8a2aba78438d2296265e05
78438d2296265e0523488ce800bae7f6d5047dbd5fb3629b78438d2296265e05
70652a97500b6a596153addd8ee8642f78438d2296265e0578438d2296265e05
```

After offset 72 the pcap stream is DES; the first DES frames are:

```text
0a03 len20
0a03 len20
0a03 len20
1703 len20
8822 len28
042b len20
042b len20
6c0e len12
0452 len28
0452 len28
0a03 len20
0452 len28
```

## 2. Host actual 6020 TX (384B capture)

Host `GAME-RAW 6020 tx` captured exactly 384 bytes:

```text
aabe80874dcd7c9141202fe1d0858dc7147302911860fcef9c78b1ba9e3e58a9
6e6e02d127f25370b8ce23f76b974201d87a2872cb2df853902f3ad87a6e2cd
380717e0b1da7677bdd5cb27457f0926892d93044cca05bb54b0d26498f92eab
f63637eca91ef09f74dec6781b1e5534c4b0d26498f92eabf63637eca91ef09f
74dec6781b1e5534c4b0d26498f92eabfbb816fd7fa255c654b0d26498f92eab
f4b0d26498f92eabf4b0d26498f92eabf410dc767b2d589d08e07fe5d4e68b9
894b0d26498f92eabf4b0d26498f92eabf4abb1fad2b3df4a2e20511309dee96
514b0d26498f92eabf90d184138abdb3354cb5851c9c4de9974b0d26498f92ea
bf4b0d26498f92eabfcf6a96700675c3f74b0d26498f92eabf4b0d26498f92ea
bf95c0830f0587b56d12ec8669ca647b504b0d26498f92eabf4b0d26498f92ea
bf76d34f1fdd9df8939b0b6683d92ef0324b0d26498f92eabf34946f8e87a8d3
499b0b6683d92ef0324b0d26498f92eabfe10a6bfd253882e7de092a4a269687e1
```

Host prefix first 72B decodes with the 6020 RC5 key to the same structural
frames `68/78` + `68/05`. The ciphertext is not byte-identical to pcap because
the session nonce/timestamp and DES session key are different; this is expected
and is not a missing prefix.

Host DES client frame logs after the prefix:

```text
GAME-CLIENT 6020 cmd=0a03 len=20 (x3)
GAME-CLIENT 6020 cmd=1703 len=20
GAME-CLIENT 6020 cmd=8822 len=28
GAME-CLIENT 6020 cmd=042b len=20
GAME-CLIENT 6020 cmd=042b len=20
GAME-CLIENT 6020 cmd=6c0e len=12
```

These commands/lengths match the pcap sequence. Payload ciphertext differs
because this is a different live session; no plaintext account/password/ticket
is logged.

## 3. Server / scene status

```text
ZG335LOGIN: LOGIN-OK accid=1 game=45.124.78.8:6020 ticket=260B
ZG335GAME: GAME-OK 6020 cmd=0a01 len=18 encrypted=des
```

The server’s first observed frame is `0a01`. The UI reached the role list and
烈火教父 was selected; “进入游戏” was clicked. It remained at “正在载入地图…”.
No `05/0c`, `05/01`, or `05/07` was observed/accepted; no `SCENE-OK` was
emitted. The first missing server command is **`050c`** (scene/map snapshot),
not a missing client prefix/frame.

The 72-byte pcap prefix is confirmed as RC5 handshake, not an opaque ticket
prefix; the host sends its own equivalent `68/78` + `68/05`. No raw pcap prefix
substitution or key replay was attempted.

## 4. Post-6c0e client tail

The pcap payloads after `6c0e` are:

```text
0452 len28 seq0: 04526753000000000000000000000000000000000000000000000000
0452 len28 seq1: 0452357a000001000000000000000000000000000000000000000000
0a03 len20:      0a03367a0000367a000000000000000000000000
0452 len28 seq2: 045240a100000200000000000000000000000000000000000000000000
```

The host now detects the outgoing `6c0e` and emits the same four-frame tail
using the current session DES key, with heartbeat sequence/timestamp fields:

```text
GAME-CLIENT 6020 cmd=6c0e len=12
GAME-TAIL 6020 order=0452,0452,0a03,0452
```

After this tail the 6020 reader was kept alive. The UI still showed
“正在载入地图… / 仍在等待游戏数据或资源”; no `05/0c` arrived and no
`SCENE-OK` was emitted. First missing server command remains **`050c`**.
The server side produced only the already-reported first `0a01`; no new
server command was observed beyond it in this run.

## 5. `68/05` plaintext field diff

Both pcap and host first-72B prefixes decode with the 6020 RC5 static key to
`68/78` (20B body) + `68/05` (80B body). The `68/05` fields are:

| Field | pcap | host | result |
|---|---:|---:|---|
| timestamp u32 | 10772 | 39523 | differs (new session/time) |
| tempId u32 | 3175 | 3250 | differs (new 7000 ticket) |
| accid u32 | 1 | 1 | equal |
| account length | 6 | 6 | equal |
| account first/last 2 | `ss` / `cc` | `ss` / `cc` | equal |
| password field length | 18 | 18 | equal (content not printed) |
| userType u16 | 0 | 0 | equal |
| ticket-field length | 48 | 48 | equal |
| ticket-field SHA-256 first 8 | `8d749e5f` | `8d749e5f` | equal |

The `68/05` body is not empty and carries the same accid/account/password
shape and ticket-field hash. `tempId`/timestamp differ because the host has a
fresh 7000 session; this is expected. No key/ticket/password content is
printed.

Conclusion: the 68/05 handoff is consistent; the remaining missing server
command is still `050c`, not a 68/05 ticket/tempId/accid bug.

## 6. Bounded 6020 retry

One retry after 1s only on 6020 connect timeout; exact logs
`GAME-RETRY 6020 attempt=2 reason=timeout` and
`GAME-FAIL 6020 attempt=2 reason=timeout possible-old-client-session` on second
failure. Receive polling is not retried; timeout is not success; ticket/DES is
not skipped. This run did not trigger a retry.

## 5. APK / release

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `app-debug.apk` | 55,832,008 | `9df18da7f8d468fb9ac00a6d65833509390aa420a3882ecd84faa0a69ea901ac` |
| `app-release-unsigned.apk` | 54,782,690 | `5bd21377fef7474b81faef2e795d19d26364dd97b6e1a1ac0f81bb6cea579b86` |

Keystore scan: debug has expected `res/raw/zg_test_keystore.p12`; release has
0 hits for `keystore|zg_test|.p12|.jks`; release unsigned.

## 6. Source hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `DirectTcpBridge.java` | 12434 | `504d46bce8a17c175e2c35e717339480869828adc5f6fcb2254959fb306082ad` |
| `Game6020Monitor.java` | 2333 | `dc74b59d7b7a63888c1b4e072e34597d2ac7e616be5c2f765dde69b6d03679d4` |
| `Game6020ClientMonitor.java` | 3340 | `bdb9771dc652b20d2a3dd0bfa5d436b6f77d6d807b0d67e2801fe4dbe46085a0` |

No pcap/key/ticket/account/password content committed; temporary blobs and
credentials deleted. No view51/client.js/protocol/old Java/assets changes; no
push.
