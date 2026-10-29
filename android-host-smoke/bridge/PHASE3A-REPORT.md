# C Render Control Packet Phase 3A Report

Task: T49. Scope: minimal C-side FULL/CONTROL render packet generation. No game
fields, JNI, Java, JS or assets were changed.

## Implementation

`zg_bridge_request_full()` sets a pending full request.

`zg_bridge_poll_render()` is nonblocking and, when pending and capacity is
sufficient, writes one 24-byte CONTROL envelope:

| Offset | Field | Value |
|---:|---|---|
| 0 | magic | `0x5A47` little-endian (`47 5A`) |
| 2 | version | `1` |
| 3 | kind | `3` (`ZG_BRIDGE_KIND_CONTROL`) |
| 4 | flags | `0` |
| 6 | header_bytes | `24` |
| 8 | frame_id | monotonic, starts at 1 |
| 12 | base_frame_id | `0` |
| 16 | payload_bytes | `0` |
| 20 | record_count | `0` |
| 22 | reserved | `0` |

Behavior:

- no pending request: `ZG_BRIDGE_AGAIN`, `out_length=0`;
- pending but `capacity < 24`: `ZG_BRIDGE_OVERSIZE`, pending stays set;
- success: `ZG_BRIDGE_OK`, `out_length=24`, pending cleared,
  `render_packets++`;
- a second poll without another `request_full` returns `AGAIN`;
- `frame_id` increments on each successful packet.

No parser behavior, caps, RX/TX policy, Phase0 ABI fields/offsets, Java, JS or
assets were changed.

## Test

`tests/test_render.c` creates a bridge, verifies empty poll, request-full,
short-cap OVERSIZE, exact envelope fields, second-poll AGAIN,
`render_packets==1`, then a second request and `frame_id==2`,
`render_packets==2`.

## MuMu execution

MuMu `emulator-5554`, `mumu-cli adb --vmindex 0`:

```text
BRIDGE-ABI PASS
ABI_EXIT=0
FRAME-PASS tokens=5 keys=2
FRAME_EXIT=0
RENDER-PASS
RENDER_EXIT=0
```

The existing ABI and parser suites remain green.

## Identity

| File | Bytes | SHA-256 |
|---|---:|---|
| `src/zg_bridge.c` | 24501 | `326c804aaf777d52822c7026b50e87109df17d2108786d6a351a02e3334e1c35` |
| `include/zg_bridge.h` | 3988 | `0419eea0a3d051ed02174960be86226e3d1ae39c8becf82d0ef998e31474af00` |
| `tests/test_render.c` | 2837 | `18c7bd9cab9cc7b7bde20d8437f08a6710a15805d5207c07a89327168f2f2adf` |
| `tests/test_frame.c` | 15794 | `126bc54b1af4c78ad2bcac3a44ca21eb41fe7b7264ca1b6b79de893210bbec4c` |
| `tests/test_abi.c` | 6587 | `0e68581bee251af4cf16e287510940bff6b193faf7320eca5327e341c1ee5ff6` |
| `CMakeLists.txt` | 1525 | `3e8a7d1678976ab968933ae0cbb704ff189509a51fae46fb2d91d7a99a285fdd` |
| `build-x86_64/zg_bridge_test_render` | 37152 | `90d7f216e6e24305c008e5fee5baaf63c6e15da272990b4d65b05b9504fca4d7` |
| `build-x86_64/zg_bridge_test_abi` | 47920 | `7a66b614d9787848fc6069932eb845fc219699c2b97483917a9e3021138a0379` |
| `build-x86_64/zg_bridge_test_frame` | 86264 | `5551a4e43e85f82a589d38d2d420d2111e24035669ffe99a8d2b9d18a3654234` |
| `build-x86_64/libzg_bridge.a` | 57006 | `a723f2c72bfbddaded5a63d1d3807a2111d6375424ad5a7ef9d0360402db4afe` |

The T47 parser identity is superseded only by the Phase3A source extension
above. Parser caps/backpressure semantics and tests are unchanged.

## Boundary

This produces only a zero-payload CONTROL packet. It does not emit game entity
records, world JSON, JNI/JS data, or a real full scene. No push was performed.
