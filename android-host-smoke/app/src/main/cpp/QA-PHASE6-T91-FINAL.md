# QA Phase 6 T91 Final Signoff — Offline SCENE-OK Replay

- Audit date: 2026-10-30
- HEAD: `22424da` (pushed)
- Test: `android-host-smoke/app/src/test/java/com/zg335/host/PcapT91B.java`
- Report: `android-host-smoke/app/src/main/cpp/PHASE6-T91D-PCAP.txt`
- Pcap used for independent run: `E:/zg-handoff-workbuddy/zg-login-7000-6020-20261029.pcap` (external; not committed)
- Mode: source/raw-log review + independent JDK compile/run; no product/tests/Java/JS changes; no push.

## 1. Verdict

**PASS.**

The independent run reproduces `FRAMES=514`, `SCENE-MAP map=2 filename=xianyangcheng`, `SELF_FRAME uid=1 name=烈火教父`, and `SCENE-OK map=2 self=烈火教父 npc=10`. The parser reassembles SLL server→client segments by TCP sequence and advances frames by `4+len`, not 8-byte padding. The commit contains no pcap/key/ticket data.

One non-product build note: the test source contains UTF-8 literals and must be compiled with `-encoding UTF-8`. Default Windows javac GBK decoding produced a false `SELF_OK=false/NOT_READY` in the first attempt; recompiling with UTF-8 produced the PASS output. This is a tooling/encoding issue, not a pcap or scene-evidence failure.

## 2. Independent execution

Compiled with the project JDK (`jdk-17.0.16.8-hotspot`) and `-encoding UTF-8`, then ran against the external pcap with the external key argument (key not printed or committed):

```text
SEGMENTS=68
S2C=23064
MAP_FRAME map=2 file=xianyangcheng len=55
SELF_FRAME uid=1 name=烈火教父 len=405
SELF_FRAME uid=1 name=烈火教父 len=405
SELF_FRAME uid=1 name=烈火教父 len=409
SELF_FRAME uid=1 name=烈火教父 len=437
SELF_FRAME uid=1 name=烈火教父 len=437
FRAMES=514
SCENE-MAP map=2 filename=xianyangcheng
SELF_OK=true
SCENE-OK map=2 self=烈火教父 npc=10
```

The first compile without `-encoding UTF-8` produced `SELF_OK=false; SCENE-OK NOT_READY`; that was source-literal decoding, not a replay data failure.

## 3. Parser checks

`PcapT91B.java`:

- Reads pcap global header at offset 24; each packet has 16-byte SLL header (`off += 16`).
- Parses IPv4 at SLL offset 16, TCP header, filters server port `6020`.
- Reassembles server→client segments in a `TreeMap<Long, byte[]>` keyed by TCP sequence; skips duplicate/overlapped bytes.
- Uses server frame size `total = 4 + len` (line 31), not 8-byte padding, and increments `o += total`.
- Handles `FLAG_ZLIB` bodies via `InflaterInputStream`.
- Map: `05/0c`, map id at payload offset 12 and filename at 22/33.
- Self: `05/01`, uid at 6 and GBK name at 38/32.
- NPC: `05/07`, unique ids at 6; count = unique set size.
- `SCENE-OK` is an offline assertion over map/self/npc (test output), not a live observer packet.

The report's `FRAMES=514`, map/self/npc result and the independent output agree.

## 4. Commit hygiene

`git show --format= --name-only 22424da`:

```text
android-host-smoke/app/src/main/cpp/PHASE6-MICRO.md
android-host-smoke/app/src/main/cpp/PHASE6-T91D-PCAP.txt
android-host-smoke/app/src/main/java/com/zg335/host/Game6020Monitor.java
android-host-smoke/app/src/test/java/com/zg335/host/PcapT91B.java
```

No pcap, key, or ticket file is committed. `PcapT91B` takes the pcap path and key as arguments and writes neither to the repository.

## 5. T90 regression / release

- T90 artifact log `PHASE6-COLD-RUN-T88.log` is not part of the T91D commit; its last commit remains `f1a2f94`. T90 functional evidence is unaffected.
- Current release APK SHA-256: `1672ecfbd98e40f21fbcdddd495c4e8c746f34bc82538b1a1c71257520e35911`
- Release scan `keystore|zg_test|.p12|.p10|.jks`: **0 matches**
- `view51.js` SHA `6fc9b01d...`, `client.js` SHA `04b5ed96...`: unchanged.
- No token/password/resource-content logging in the T91D source/report; the test key is runtime-only.

## 6. Findings summary

| Item | Result |
|---|---|
| SLL/seq reassembly | PASS: 68 segments / 23064B |
| Frame advance `4+len` not 8B | PASS |
| FRAMES=514 | PASS |
| SCENE-MAP map2/xianyangcheng | PASS |
| SELF_FRAME uid1/烈火教父 | PASS |
| SCENE-OK map2/self/npc10 | PASS (offline replay assertion) |
| pcap/key/ticket not committed | PASS |
| T90 evidence unaffected | PASS |
| release p10/p12/keystore | PASS: 0 |
| view51/client/protocol | PASS: unchanged |

## 7. Boundary

This is offline pcap replay evidence, not a live server run. The `SCENE-OK` is the replay test's derived assertion, not a `GAME-OK` live observer line; this distinction is explicit. No product source, tests, Java or JS was modified; no push was performed.
