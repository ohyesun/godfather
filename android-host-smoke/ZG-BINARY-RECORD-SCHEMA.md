# ZG Binary Record Schema (Frozen Draft v1)

Status: **document-only freeze for Phase 3D**. No C/Java/JS/assets changes in
this task. The current C test record `0x7F01` is **test-only** and will be
replaced by this schema; it must not ship.

Field inventory: **65 field definitions** across 6 record types
(MAP 2, SELF 19, ACTOR_UPSERT 27, ACTOR_REMOVE 0 payload fields, EVENT 13,
HUD 4), plus the common 12-byte record header.

Inputs:

- `android-host-smoke/ANDROID-JS-DATA-PATH.md` (T36)
- `app/src/main/assets/frame109.js`
- `app/src/main/assets/client.js`
- `app/src/main/assets/view51.js`
- `app/src/main/assets/hud109.js`
- Old Java `MobileStream109` gzip JSON input, only as a reference for the
  full-snapshot shape. It is not used to invent offsets or field IDs.

## 1. Envelope and record header

The existing 24-byte envelope remains unchanged:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | magic `0x5A47` little-endian |
| 2 | 1 | version 1 |
| 3 | 1 | kind: 1 FULL, 2 DELTA, 3 CONTROL, 4 ERROR |
| 4 | 2 | flags (`0x0001` keyframe) |
| 6 | 2 | header_bytes = 24 |
| 8 | 4 | frame_id |
| 12 | 4 | base_frame_id |
| 16 | 4 | payload_bytes |
| 20 | 2 | record_count |
| 22 | 2 | reserved = 0 |

Every record starts with the existing 12-byte header:

| Record offset | Size | Field |
|---:|---:|---|
| 0 | 2 | record_type |
| 2 | 2 | schema = 0 |
| 4 | 4 | entity_id |
| 8 | 4 | payload_bytes |

`record_type` values:

| Value | Type | Meaning |
|---:|---|---|
| `0x0101` | MAP | map identity/scene metadata |
| `0x0102` | SELF | local player baseline |
| `0x0103` | ACTOR_UPSERT | actor add/replace |
| `0x0104` | ACTOR_REMOVE | actor removal |
| `0x0105` | EVENT | append-only event |
| `0x0106` | HUD | HUD-only scalar state |
| `0x7F01` | TEST_ONLY | current C test record; replace, never production |

## 2. Payload field encoding

Payload is a sequence of TLV fields:

```text
field_id   u16 LE
type       u8     (1=u8, 2=u16, 3=u32, 4=i32, 5=f32, 6=UTF-8 string)
flags      u8     (bit0 optional-present; 0 for this schema)
value_len  u16 LE
value      value_len bytes
```

Field size is `6 + width`; string width is `value_len` bytes. Unknown fields are
skipped by `value_len`; a truncated field makes the record invalid. `value_len`
is the string length prefix for UTF-8 strings; there is no NUL terminator.
Every string is UTF-8, maximum 4096 bytes; names should stay <=255 bytes. The
decoder must reject invalid UTF-8 and overlength strings.

Field IDs are scoped by record type but intentionally reuse common IDs.

## 3. MAP record (`0x0101`, entity_id = 0)

| ID | Field | Type | Width | Unit/meaning | Req. | Old JS source |
|---:|---|---|---:|---|---|---|
| `0x0100` | `map_id` | u32 | 4 | map/nav id; same numeric id used for `navigation109` | yes | `view51.js:764`, `hud109.js:190` |
| `0x0101` | `map_name` | string | len | display name | no | `hud109.js:213` |

`instanceId`, `country`, and scene-token fields are not frozen here; see TODO.

## 4. SELF record (`0x0102`, entity_id = player uid)

The record header `entity_id` maps to the old self `player.uid`.

| ID | Field | Type | Width | Unit/meaning | Req. | Old JS source |
|---:|---|---|---:|---|---|---|
| `0x0002` | `name` | string | len | label/portrait identity | yes | `view51.js:812` |
| `0x0003` | `visual_model` | string | len | model/visual id; not a numeric id | yes | `view51.js:690`; compare `hud109.js:4` |
| `0x0004` | `appearance_id` | u32 | 4 | portrait selector | no | `hud109.js:4` |
| `0x0005` | `profession` | u8 | 1 | 1=shennong, 2=yinyang, 3=bing | no | `hud109.js:6`, `view51.js:727` |
| `0x0006` | `sex` | u8 | 1 | 1=female, 2=male | no | `hud109.js:6`, `view51.js:727` |
| `0x0010` | `x` | f32 | 4 | world x; same units as old world | yes | `view51.js:764`, `view51.js:811` |
| `0x0011` | `y` | f32 | 4 | vertical position; optional, default 0 | no | `view51.js:811` |
| `0x0012` | `z` | f32 | 4 | world z; same units as old world | yes | `view51.js:764`, `view51.js:811` |
| `0x0013` | `dir_deg` | f32 | 4 | facing degrees; old code converts with `sin(dir*PI/180)` | no | `view51.js:304-306` |
| `0x0014` | `move_speed` | f32 | 4 | movement speed; default 5 in old predictor | no | `view51.js:615` |
| `0x0022` | `dead` | u8 | 1 | 0/1; blocks movement/actions | yes | `view51.js:812` |
| `0x0026` | `actions_blocked` | u8 | 1 | 0/1 | no | `view51.js:811` |
| `0x0027` | `sitting` | u8 | 1 | 0/1 | no | `view51.js:813` |
| `0x0028` | `weak` | u8 | 1 | 0/1 | no | `view51.js:813` |
| `0x0029` | `stalling` | u8 | 1 | 0/1 | no | `view51.js:813` |
| `0x002A` | `route_active` | u8 | 1 | 0/1 | no | `view51.js:812` |
| `0x002B` | `route_target_x` | f32 | 4 | route target x | no | `view51.js:812` |
| `0x002C` | `route_target_z` | f32 | 4 | route target z | no | `view51.js:812` |
| `0x0007` | `level` | u16 | 2 | character level for HUD | no | `hud109.js:195` |

Self HP/maxHP is not directly read by the inspected self-render path; it is a
TODO below rather than a guessed field.

## 5. ACTOR_UPSERT (`0x0103`, entity_id = actor id)

`actor_kind`: `1=player`, `2=mob`, `3=npc`, `4=drop`. The old renderer selects
the list by array membership (`players`, `mobs`, `npcs`, `drops`) at
`view51.js:816`; actor creation is at `view51.js:778-789`.

| ID | Field | Type | Width | Unit/meaning | Req. | Old JS source |
|---:|---|---|---:|---|---|---|
| `0x0001` | `actor_kind` | u8 | 1 | 1 player, 2 mob, 3 npc, 4 drop | yes | `view51.js:816`, `view51.js:778` |
| `0x0003` | `visual_model` | string | len | model/visual id | no | `view51.js:782`, `view51.js:798` |
| `0x0002` | `name` | string | len | label/name | yes for player/mob/npc | `view51.js:786`, `view51.js:805` |
| `0x0007` | `level` | u16 | 2 | mob level label | no | `view51.js:786` |
| `0x0010` | `x` | f32 | 4 | world x | yes | `view51.js:783`, `view51.js:788` |
| `0x0012` | `z` | f32 | 4 | world z | yes | `view51.js:783`, `view51.js:788` |
| `0x0020` | `hp` | i32 | 4 | current HP; no sentinel | no | `view51.js:786`, `view51.js:788` |
| `0x0021` | `max_hp` | i32 | 4 | max HP | no | `view51.js:786` |
| `0x0022` | `dead` | u8 | 1 | 0/1; hides actor | no | `view51.js:788` |
| `0x0023` | `hp_known` | u8 | 1 | 0 means suppress HP label | no | `view51.js:786`, `view51.js:788` |
| `0x0024` | `alive` | u8 | 1 | 0/1; targeting check | no | `view51.js:171` |
| `0x0025` | `hidden` | u8 | 1 | 0/1; skips actor creation | no | `view51.js:778` |
| `0x0030` | `boss` | u8 | 1 | 0/1; fallback actor color | no | `view51.js:781` |
| `0x0031` | `count` | u32 | 4 | drop stack count | no | `view51.js:786` |
| `0x0032` | `icon` | string | len | drop icon URL/name | no | `view51.js:786` |
| `0x0033` | `quality_color` | string | len | drop label color | no | `view51.js:786` |
| `0x0034` | `name_color` | string | len | mob name color | no | `view51.js:788` |
| `0x0035` | `grade` | string | len | mob grade label | no | `view51.js:788` |
| `0x0036` | `teammate` | u8 | 1 | 0/1; player label suffix | no | `view51.js:805` |
| `0x0037` | `quest_ready` | u8 | 1 | 0/1; label marker | no | `view51.js:788` |
| `0x0038` | `quest_target` | u8 | 1 | 0/1; label marker | no | `view51.js:788` |
| `0x0039` | `guide_arrow` | u8 | 1 | 0/1; scene arrow | no | `view51.js:788` |
| `0x003A` | `guide_direction` | f32 | 4 | degrees for guide arrow | no | `view51.js:788`, `view51.js:636` |
| `0x003B` | `portal_effect` | u8 | 1 | 0/1; exit portal effect | no | `view51.js:782`, `view51.js:788` |
| `0x003C` | `exit` | u8 | 1 | 0/1; exit marker | no | `view51.js:783` |
| `0x003D` | `mount_visual_model` | string | len | mount companion visual | no | `view51.js:801-803` |
| `0x003E` | `pet_visual_model` | string | len | pet companion visual | no | `view51.js:801-803` |

## 6. ACTOR_REMOVE (`0x0104`)

No payload; the record header `entity_id` is the actor id to delete. The old
delta merge deletes by id at `frame109.js:5` (`delta.remove`). Unknown ids are
ignored.

## 7. EVENT (`0x0105`, entity_id = 0)

Events are append-only and deduplicated by `event_id`; old renderer increments
`this.seen` and skips `e.id<=this.seen` at `view51.js:767`.

| ID | Field | Type | Width | Unit/meaning | Req. | Old JS source |
|---:|---|---|---:|---|---|---|
| `0x0050` | `event_id` | u32 | 4 | monotonic event id | yes | `view51.js:767` |
| `0x0051` | `event_type` | u8 | 1 | enum below | yes | `view51.js:768-775` |
| `0x0052` | `uid` | u32 | 4 | source actor id | conditional | `view51.js:769-773` |
| `0x0010` | `x` | f32 | 4 | source x | conditional | `view51.js:770-772` |
| `0x0012` | `z` | f32 | 4 | source z | conditional | `view51.js:770-772` |
| `0x0057` | `to_x` | f32 | 4 | cast target x | conditional | `view51.js:772` |
| `0x0058` | `to_z` | f32 | 4 | cast target z | conditional | `view51.js:772` |
| `0x0053` | `damage` | i32 | 4 | damage number | conditional | `view51.js:771` |
| `0x0054` | `skill` | u32 | 4 | skill id | conditional | `view51.js:772` |
| `0x0055` | `animation` | u16 | 2 | cast animation id | conditional | `view51.js:772` |
| `0x0056` | `event_at` | u32 | 4 | event time ms, wraps | conditional | `view51.js:772` |
| `0x0059` | `target_kind` | u8 | 1 | 1 player, 2 mob | no | `view51.js:681` |
| `0x005A` | `source_kind` | u8 | 1 | 1 player, 2 mob | no | `view51.js:681` |

Event type enum:

| Value | Type | Old JS source |
|---:|---|---|
| 1 | impact | `view51.js:768` |
| 2 | cast-stop | `view51.js:769` |
| 3 | cast-aim | `view51.js:770` |
| 4 | damage | `view51.js:771` |
| 5 | cast | `view51.js:772` |
| 6 | dodge | `view51.js:773` |
| 7 | jump | `view51.js:773` |
| 8 | motion-result113 | `view51.js:775` |

## 8. HUD (`0x0106`, entity_id = 0)

HUD is independent of the scene baseline. In FULL it carries current values;
in DELTA only present fields change. Missing values mean "unavailable", not
zero.

| ID | Field | Type | Width | Unit/meaning | Req. | Old JS source |
|---:|---|---|---:|---|---|---|
| `0x0060` | `gold` | i32 | 4 | gold count | no | `hud109.js:190`, `hud109.js:195` |
| `0x0061` | `yuanbao` | i32 | 4 | yuanbao count | no | `hud109.js:190`, `hud109.js:195` |
| `0x0062` | `vip_level` | u16 | 2 | VIP level | no | `hud109.js:195` |
| `0x0007` | `level` | u16 | 2 | player level | no | `hud109.js:195` |

`buffs` is explicitly hidden for the PC bridge at `hud109.js:190`; it is not
part of this minimal HUD record. `channel`, `food`, `gather`, and `ladder` are
listed in the T36 ownership notes but are not read by the inspected
`hud109.js:190` update; they are TODO, not guessed fields.

## 9. FULL / DELTA semantics

### FULL (kind 1)

- `base_frame_id = 0`; the frame is a complete baseline.
- Exactly one MAP record is required for a renderable scene.
- Exactly one SELF record is required.
- All currently live actors should be emitted as ACTOR_UPSERT records.
- HUD is optional; absent means unavailable/zero UI values are not implied.
- Omitted actors are not live after the baseline is applied.
- `record_count` and `payload_bytes` must exactly match emitted bytes.

### DELTA (kind 2)

- `base_frame_id` is the previously applied frame id; mismatch is a resync
  error, matching `frame109.js:3`.
- ACTOR_UPSERT replaces the actor record for that entity id (full actor
  snapshot, not a field patch).
- ACTOR_REMOVE deletes by header entity id.
- EVENT records append; consumer keeps a bounded event history and dedupes by
  `event_id` (`view51.js:767`).
- HUD records are independent patches; omitted HUD fields are unchanged.
- MAP in DELTA means scene change; consumer must reset scene-local actor state
  and request/await a FULL baseline.
- Unknown record types are skipped by `payload_bytes`; unknown TLV fields are
  skipped by `value_len`.

## 10. Numeric precision and ranges

- Coordinates and route targets: `f32`, no quantization; same world units and
  coordinate system as old `view51` x/z; preserve negative values. Valid world
  bounds are not frozen.
- `dir_deg` / `guide_direction`: `f32` degrees; old code multiplies by
  `PI/180` (`view51.js:305-306`, `view51.js:636`). Normalize to `[0,360)` at
  encode; decoder accepts finite values.
- HP/maxHP: `i32`; `0` is a valid HP value and must not be used as "unknown".
  Use `hp_known=0` when HP is unknown. Old renderer displays HP for actors when
  `hpKnown!==false` and tests `hp>0` at `view51.js:786-788`.
- `count`: `u32`; drop stacks only.
- Event time: `u32` milliseconds, wrapping after 49.7 days; old `at` is only
  used as a local animation timestamp (`view51.js:772`).
- Visual id: UTF-8 string (`visual_model`), not a numeric offset. `appearance_id`
  is a separate u32 for the HUD portrait selector (`hud109.js:4`).
- No field is allowed to use a guessed legacy byte offset. This schema assigns
  new field IDs; the old JSON has no stable binary offsets.

## 11. Minimal renderable FULL example (test data, no account data)

Example: map id `1`, self uid `1`, name `"T"`, x `1.0`, z `2.0`, visual
`"m"`, dir `0.0`, level `1`, dead `0`; no HUD/actors. TLV header is 6 bytes;
type codes: u8=1, u16=2, u32=3, f32=5, string=6.

Envelope:

```text
47 5A 01 01 01 00 18 00 01 00 00 00 00 00 00 00
5D 00 00 00 02 00 00 00
```

`payload_bytes=0x5D` (93), `record_count=2`.

MAP record at offset 24:

```text
type=01 01  schema=00 00  entity=00 00 00 00  payload=0A 00 00 00
map_id TLV: 00 01 03 00 04 00 01 00 00 00
```

SELF record at offset 46:

```text
type=02 01  schema=00 00  entity=01 00 00 00  payload=3B 00 00 00
name:         02 00 06 00 01 00 54
x:            10 00 05 00 04 00 00 00 80 3F
z:            12 00 05 00 04 00 00 00 00 40
visual_model: 03 00 06 00 01 00 6D
dir_deg:      13 00 05 00 04 00 00 00 00 00
level:        07 00 02 00 02 00 01 00
dead:         22 00 01 00 01 00 00
```

Total = 24 envelope + 22 MAP/record + 71 SELF/record = 117 bytes. This is a
**test fixture**, not a real account frame.

## 12. Unknown / not frozen

The following are intentionally TODO; do not invent offsets or IDs:

- MAP `instanceId`, `country`, scene-token, map dimensions/bounds.
- SELF HP/maxHP, inventory/equipment/skills/state, full `visual` parts,
  textures, weapons, scale, effects (`view51.js:205` mentions the visual key
  but not a minimal encoding).
- ACTOR y coordinate, general facing, full visual part list, advanced
  appearance, `native` model payload.
- Companion/mount/pet fields beyond optional visual-model strings; mount status
  and companion state are not frozen.
- EVENT optional-field presence rules per event type; exact `at` epoch/timebase;
  event batching bounds beyond append/dedupe.
- HUD `buffs`, `channel`, `food`, `gather`, `ladder`; actual HUD ownership is
  not proven by the inspected `hud109.js:190` update.
- String maximums beyond the global 4096-byte cap and per-name recommendation.
- World coordinate bounds, HP numeric bounds, and skill/animation ID ranges.
- Whether ACTOR_UPSERT is full replacement or field patch in future contract
  versions; v1 freezes full replacement.
- `0x7F01` replacement timing and the PC contract record IDs.

## 13. Boundary

This is a schema freeze for test adapters. It does not change the C ABI,
parser, JNI, Java, JS, Three.js, client.js, assets, or production trust. The
existing `0x7F01` C test record is test-only and must be replaced before any
production claim. No push was performed.
