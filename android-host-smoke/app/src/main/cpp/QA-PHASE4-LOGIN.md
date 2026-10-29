# QA Phase 4 — Direct 7000 Login

- Audit date: 2026-10-28
- Scope: `DirectTcpBridge.java`, `Login7000Monitor.java`, `MainActivity.java`, current debug APK, MuMu device state
- Mode: read-only source and device observation; no product/tests/Java/JS changes; no push.
- Current debug APK SHA-256: `f4bc2b42fa69c57c1ad6dd3c3c544c17512d9116687a8de0aaec0631b70cfd91` — matches the task.

## 1. Verdict

**PASS for the real direct-7000 login and role list; CONCERN for the 6020 first-connect timeout/session contention.**

The QA evidence is re-signed against the corrected T72 report at `4e1616d`; that commit changes only `PHASE4-LOGIN-REPORT.md` (8 lines), while the three Java source hashes remain unchanged. The 6020 first-connect timeout was reproduced and is correctly recorded by the author as a session/old-APK contention CONCERN, not as a PASS condition or an RC5 failure.

The device is on the character selection screen with `烈火教父 神农26级` and `战国七雄 神农1级`. The log contains only login metadata (`accid`, game endpoint, ticket length); no password or ticket content is printed. No hardcoded account/password/ticket literals are present in the three inspected Java files.

## 2. Credential and log hygiene

`DirectTcpBridge.java`:

- `request95` accepts `credential`/`account` from the JS caller; it does not contain a literal account/password.
- It logs only route/status and `open host:port`, not the request body or credentials.
- `decodeHex`/`encodeHex` handle packet data but do not log packet content.

`Login7000Monitor.java`:

- The only emitted login line is:

```text
LOGIN-OK accid=... game=45.124.78.8:6020 ticket=260B
```

- `ticket=` is the **length** only; no ticket bytes.
- It contains a fixed RC5 protocol key for decrypting the server response, not an account/password/ticket. The real server response and character list demonstrate that the chain is not a fake-key stub.
- No `password`/`passwd`/`pwd` logging exists in the inspected files.

Static checks on `DirectTcpBridge.java`:

```text
password 0, passwd 0, pwd 0, ticket 0, account parameter only (1 declaration)
```

No credential literal was found.

## 3. Real device log

Actual logcat from `emulator-5554`:

```text
I/ZG335DIRECT: open 45.124.78.8:7000
I/ZG335DIRECT: route=/direct/open status=200
I/ZG335LOGIN:  LOGIN-OK accid=1 game=45.124.78.8:6020 ticket=260B
I/ZG335DIRECT: open 45.124.78.8:6020
E/ZG335DIRECT:  java.net.SocketTimeoutException: failed to connect to /45.124.78.8 (port 6020) ... after 6000ms
I/ZG335DIRECT: open 45.124.78.8:7000
I/ZG335DIRECT: route=/direct/open status=200
I/ZG335LOGIN:  LOGIN-OK accid=1 game=45.124.78.8:6020 ticket=260B
I/ZG335DIRECT: open 45.124.78.8:6020
I/ZG335DIRECT: route=/direct/open status=200
```

This reproduces the 6020 first-connect timeout and the subsequent successful retry. A separate unrelated `45.124.78.3:7000` timeout also appears in the history; the target four-zone login is `45.124.78.8`.

No password/ticket content appears in the log.

## 4. Role-list evidence

Screenshot captured from the device: `E:\zg-handoff-workbuddy\tmp-t44\qa-phase4-login.png`.

Visible character-selection screen:

```text
选择英雄角色
烈火教父
神农 26级

战国七雄
神农 1级

新建角色 / 空角色位
```

This confirms the session reached the real character-list page, not merely a login-OK log line.

## 5. Source hygiene and scope

`git show --stat ef7500a` changed only:

- `app/src/main/java/com/zg335/host/DirectTcpBridge.java`
- `app/src/main/java/com/zg335/host/Login7000Monitor.java`
- `MainActivity.java` (injection wiring)
- `app/src/main/cpp/PHASE4-LOGIN-REPORT.md`

`view51.js`, `client.js`, the binary protocol/record layer and old assets were not modified by T72.

## 6. Release hygiene

Current release APK:

- SHA-256: `432142ea70c8a7245726458d2fddcd7bc9c1b7538502548887527b50db0419eb`
- Zip scan `keystore|zg_test|.p12|.jks`: **0 matches**

## 7. Findings summary

| Item | Result |
|---|---|
| No hardcoded account/password/ticket | PASS |
| No password/ticket content logged | PASS: ticket length only |
| Real 45.124.78.8:7000 login | PASS |
| LOGIN-OK metadata | PASS: accid1 / game 6020 / 260B |
| 6020 first timeout + retry success | CONCERN: session contention, reproduced |
| Character list | PASS: 烈火教父26 / 战国七雄1 |
| 68/78/0f/10/02/04 real chain | PASS by successful real login/character list; no fake-key/false account |
| view51/client/protocol/assets unchanged | PASS: commit scope |
| release p12/keystore | PASS: 0 |

## 8. Report revision `4e1616d`

The corrected T72 report at `4e1616d` was independently reviewed:

- It changes only `PHASE4-LOGIN-REPORT.md` (8 insertions/1 deletion), not the three Java source files.
- The corrected report now says the commit was pushed as `ef7500a`; the stale “no push” line is removed.
- It records the 6020 first-connect timeout as old-APK/session contention, not as an RC5/68-04 failure; this matches the observed timeout followed by a successful retry.
- Source hashes remain `ad216a69...` / `4a510ddd...` / `bf57e5c3...`; the role-list screenshot and APK evidence are unchanged.

## 9. Boundary

This is local WebView/socket/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, whole-game acceptance, or credential security beyond the checked log/code paths. No product source, tests, Java or JS was modified; no push was performed.
