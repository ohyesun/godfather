# JNI Gzip Frame Feed Phase 2B Report

Task: T43. Scope: prove the C frame parser through JNI with Java-generated gzip
input. No real server, game fields, Java object parsing, or JS output.

## Files

- `app/src/debug/java/com/zg335/host/FrameFeedTest.java`
  - debug-only
  - builds `{"a":1,"b":"x"}` bytes
  - gzip via `GZIPOutputStream`
  - prepends 4-byte big-endian compressed length
  - feeds one direct `ByteBuffer` via `ZgBridge.feed`
  - logs `frame status=... rx=... frames=... error=... before=...`
- `app/src/main/java/com/zg335/host/MainActivity.java`
  - existing onCreate control feed still runs first (`rx=24/frames=1`)
  - debug reflection hook now invokes `FrameFeedTest`
- `app/src/main/cpp/CMakeLists.txt`
  - links `z` for the C gzip parser

`TlsLoopbackTest` remains debug-only but is no longer invoked by the debug hook,
so it cannot add an extra frame and distort `frames=2`.

## Build/install/run

```text
./gradlew assembleDebug
BUILD SUCCESSFUL

mumu-cli adb --vmindex 0 uninstall com.zg335.host
Success
mumu-cli adb --vmindex 0 install app-debug.apk
Success
mumu-cli adb --vmindex 0 shell am start -n com.zg335.host/.MainActivity
Starting: Intent { cmp=com.zg335.host/.MainActivity }
```

MuMu's external adb intermittently went offline; the stable path was
`mumu-cli adb --vmindex 0`.

## Logcat

```text
I ZG335HOST: state=1 normal=0 overflow=0
I ZG335BRIDGE: status=0 rx=24 frames=1 error=0
I ZG335BRIDGE: frame status=0 rx=63 frames=2 error=0 before=24
```

- initial control: 24 bytes, `frames=1`
- gzip framed input: 39 bytes (`4B BE length + gzip`), total `rx=63`
- final `frames=2`
- `status=0`, `error=0`
- process pid remained alive (3110)

## APK

- Path: `app/build/outputs/apk/debug/app-debug.apk`
- Size: 44,024,046 bytes
- SHA-256: `41fd13e5e54d6796e018a85b9dd4dd6903f05ca763ed5123c8ecc788462b0b7a`

## Binary-only boundary

Java only creates test bytes and calls `ZgBridge.feed`. No JSON parser or game
object is created in Java; the debug frame is not sent to JS. The C side handles
4-byte BE framing, gzip, and token counting. No Phase0 ABI fields/offsets were
changed.

## Hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `app/src/debug/java/com/zg335/host/FrameFeedTest.java` | 2298 | `6cda676f239b6a0194f1498139e282f391dc4633fc987abe8b0e94fb47fcdafc` |
| `app/src/main/cpp/CMakeLists.txt` | 931 | `eb78ee7c7487504cf4807e2083e832475570855dbdc14458fa17afcba78b113a` |
| `app/src/main/java/com/zg335/host/MainActivity.java` | 4586 | `dad6c4e9d40b9b5cfb5046f428e572887e9a4f528a2eb82dc28429af116a3f9d` |

No push; no Java/JS/assets protocol changes beyond the debug test hook and
zlib link.
