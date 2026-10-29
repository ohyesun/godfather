# QA Phase 3C — Binary Record Layer

- Audit date: 2026-10-28
- Scope: `bridge/src/zg_bridge.c`, `bridge/tests/test_render.c`, `app/src/main/java/com/zg335/host/MainActivity.java`, JNI render path
- Mode: read-only product source; independent NDK r27 build; MuMu `emulator-5554` run; no product/tests/Java/JS changes; no push.
- Frozen identities verified:
  - `bridge/src/zg_bridge.c`: `c48d3603d0604cde8da81eec3999bbac5f7b4bc720b8e2c62d76f30e01c93fd6` (25,362 bytes)
  - `bridge/tests/test_render.c`: `4abdd512831990099ce67bd0dc2500f73c0198bd42aaa271dbedcab3152b6efa` (3,446 bytes)
  - `MainActivity.java`: `7d38f2ef8717cb76e60f291ca41402a986009f169acda2c35233c23decb16c2e` (10,545 bytes)

## 1. Verdict

**PASS for the 40-byte binary record layer.**

The 24-byte FULL envelope + 12-byte record header + 4-byte little-endian sequence is emitted as 40 bytes, cap<40 returns OVERSIZE, empty returns AGAIN, the JS adapter parses the binary fields and logs real records, and the release package has no p12/keystore material. The test-only 0x7F01 record is explicitly marked test-only and is not connected to a game field or client.js/Three.js path.

## 2. 40-byte layout

Source `bridge/src/zg_bridge.c`:

- `ZG_RENDER_PACKET_BYTES = 40` (line 14)
- `ZG_TEST_RECORD_TYPE = 0x7f01` (line 15)
- `ZG_TEST_RECORD_PAYLOAD_BYTES = 4` (line 16)
- `zg_bridge_poll_render()` at lines 727-777:
  - envelope magic/version/kind (`FULL=1`)
  - flags `KEYFRAME`
  - header_bytes `24`
  - frame_id increment at line 754-755
  - payload_bytes `12 + 4 = 16` at 757-758
  - record_count `1` at 759
  - record header at offset 24: type `0x7f01`, schema `0`, entity_id `0`, payload_bytes `4`
  - seq increment at offset 36 at 772-773
  - total length 40 at 776

The independent `test_render.c` checks:

- empty render -> `AGAIN`, length 0
- cap 39 -> `OVERSIZE`
- cap 64 -> length 40
- magic/version/kind FULL/flags/header24/frame_id1/base0/payload16/record_count1/reserved0
- record type `0x7f01`, schema0, entity0, payload4
- seq at offset36 =1
- second request -> frame_id2, seq2, render_packets2

No arithmetic overflow is reachable in the 40-byte layout calculation: all offsets/sizes are fixed constants and the sequence/frame counters are `uint32_t` increments. The test does not exercise 2^32 wrap; that is a theoretical counter wrap, not a current layout/overflow failure.

## 3. C test execution

Independently compiled with NDK r27 clang 18.0.4 for `x86_64-linux-android24` and run on `emulator-5554`:

```text
RENDER-PASS records=1 type=7F01 seq=1
EXIT=0
```

The cap<40 OVERSIZE and empty AGAIN assertions are in that same binary and passed.

## 4. JS adapter

`MainActivity.java:43-54` uses only binary byte access:

```js
fetch('/zg/render',{cache:'no-store'})
  .then(response => response.arrayBuffer())
  .then(buffer => {
    var view = new Uint8Array(buffer);
    if (view.length !== 40) return;
    var recordCount = view[20] | (view[21] << 8);
    var type = (view[24] | (view[25] << 8)).toString(16).toUpperCase();
    var seq = view[36] | (view[37] << 8) | (view[38] << 16) | (view[39] << 24);
    console.log('ZG335JS records=' + recordCount + ' type=' + type + ' seq=' + seq);
  });
```

The actual offsets match the 40-byte layout: record_count at 20, record type at 24, seq at 36. The adapter also checks status 200 and length 40 before parsing. It uses `Uint8Array` (acceptable DataView/Uint8Array binary path) and has no JSON/base64/hex/`evaluateJavascript` frame transport.

## 5. Independent MuMu result

Debug APK was built with Gradle/JDK17 and installed on `emulator-5554`. Actual logcat:

```text
I/ZG335BRIDGE: webview=110.0.5481.154.1
I/ZG335BRIDGE: status=0 rx=24 frames=1 error=0
I/ZG335BRIDGE: frame status=0 rx=63 frames=2 error=0 before=24
I/ZG335JS: ZG335JS records=1 type=7F01 seq=1 magic=5A47
I/ZG335JS: ZG335JS records=1 type=7F01 seq=2 magic=5A47
I/ZG335BRIDGE: ui loaded https://appassets.androidplatform.net/assets/index.html
I/ZG335JS: ZG335JS records=1 type=7F01 seq=3 magic=5A47
...
I/ZG335JS: ZG335JS records=1 type=7F01 seq=19 magic=5A47
```

This is actual WebView console output, not a restatement of the author log. The legacy `index.html` UI remains loaded.

## 6. Test-only record and asset integration

- Source comment at `zg_bridge.c:762-763` explicitly says `Test-only record; replace with the PC contract record later.`
- The record type constant is named `ZG_TEST_RECORD_TYPE`; no game field name/semantics are attached.
- `MainActivity.java` does not reference `client.js`, Three.js or a game field path; it only injects the binary poll adapter into the existing `index.html`.
- The `client.js` asset remains a legacy UI asset; no new record-layer hook points it at this 0x7F01 packet.
- No JSON/base64/hex/`evaluateJavascript` frame path exists in MainActivity.

## 7. Release hygiene

Current release artifact: `app/build/outputs/apk/release/app-release-unsigned.apk`

- SHA-256: `7e00f6abad85cb173dcb5ded387bb353b29b0bef82b05d06b05cca6055adba58`
- Zip entries: 1,820
- Case-insensitive scan `keystore|zg_test|.p12|.jks`: **0 matches**
- `classes.dex` contains `ZG335JS records=`, `if(view.length!==40)`, `var seq=view[36]`, `/zg/render`, and `application/octet-stream`; this is the T53 binary-record build, not an older endpoint package.
- The earlier T52 release hash was `961cd0d...`; the current T53 release hash is different because the record layer changed. No p12/keystore material is present.

## 8. Findings summary

| Item | Result |
|---|---|
| 24B envelope + 12B record + 4B seq = 40B | PASS |
| kind FULL, record_count1, payload_bytes16 | PASS |
| record type7F01/schema0/entity0/payload4 | PASS |
| seq/frame_id increment | PASS: test 1→2; device seq1..19 |
| cap<40 OVERSIZE | PASS |
| empty AGAIN | PASS |
| endpoint no-packet 204 | PASS (source path; debug device continuously polls packets) |
| JS Uint8Array offsets 20/24/36 | PASS |
| real `ZG335JS records=...` log | PASS |
| JSON/base64/hex/evaluateJavascript frame path | PASS: none |
| test-only 0x7F01 labeling | PASS: source comment/constant |
| client.js/Three.js game-field hook | PASS: none in MainActivity |
| release p12/keystore | PASS: 0 matches |
| integer overflow in layout | No reachable layout overflow; counter wrap untested |

## 9. Boundary

This is local C/JNI/WebView/APK QA on MuMu x86_64. It does not claim production runtime, iOS, signing, physical device, game-field semantics, client.js/Three.js integration, or whole-product acceptance. No product source, tests, Java or JS was modified; no push was performed.
