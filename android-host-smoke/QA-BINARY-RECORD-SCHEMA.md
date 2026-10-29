# QA Binary Record Schema

- Audit date: 2026-10-28
- Scope: `android-host-smoke/ZG-BINARY-RECORD-SCHEMA.md` and old JS sources
- Mode: read-only source/document review; no product source, document, Java/JS or asset changes; no push.
- Document SHA-256: `63410f3f3dff9a4fe676c0029266cff0331af5a9b4eea3b4153eaee9ddfd13fd` (15,259 bytes) — matches the task identity.

## 1. Verdict

**PASS for the field inventory, TLV contract and 117-byte FULL example; CONCERN for the current test-only 0x7F01 still being present in the current test/release build.**

The document has exactly 65 field definitions, every field row carries an old-JS source reference, all 82 referenced line locations are inside the named JS files, the sampled MAP/SELF/ACTOR/EVENT/HUD references match actual code, and the minimal MAP+SELF FULL example independently sums to 117 bytes.

## 2. Field count and source references

Programmatic table inspection:

- 65 field rows total:
  - MAP: 2
  - SELF: 19
  - ACTOR_UPSERT: 27
  - ACTOR_REMOVE: 0 payload fields
  - EVENT: 13
  - HUD: 4
- 82 source references across `view51.js` and `hud109.js`.
- All references are in-file:
  - `view51.js:171-816`
  - `hud109.js:4-213`
- No field row lacks a source reference.
- No cross-record field ID has conflicting type/width. The six reused IDs are scoped by record type and retain the same encoding (`name` string, `visual_model` string, `level` u16, `x` f32, `z` f32, `dead` u8).

Sample actual line checks:

| Record | Field | Reference | Actual source evidence |
|---|---|---|---|
| MAP | `map_id` | `view51.js:764` | `s.map.id` used for map/scene selection |
| MAP | `map_name` | `hud109.js:213` | `world?.map.name??state.map.name` displayed |
| SELF | `name` | `view51.js:812` | `p.name` in own label |
| SELF | `visual_model` | `view51.js:690` | `player.visual?.model` |
| SELF | `dir_deg` | `view51.js:304-306` | `Math.sin(pcDir*Math.PI/180)` / `Math.cos(...)` |
| ACTOR | `actor_kind` | `view51.js:816`, `:778` | player/mob/npc/drop lists and hidden actor path |
| ACTOR | `hp_known` | `view51.js:786`, `:788` | `n.hpKnown===false` suppresses HP label |
| ACTOR | `exit` | `view51.js:783` | `n.exit?.15` placement |
| EVENT | `event_id` | `view51.js:767` | `if(e.id<=this.seen)continue;this.seen=e.id` |
| EVENT | `event_type` | `view51.js:768-775` | impact/cast-stop/cast-aim/damage/cast/dodge/jump/motion-result branches |
| HUD | `gold` / `yuanbao` | `hud109.js:190`, `:195` | `bridgeGold`/`bridgeYuanbao` |
| HUD | `vip_level` / `level` | `hud109.js:195` | `vipLevel` and `player.level` |

The source line references are therefore not invented offsets; they point to the old renderer's actual field usage.

## 3. TLV encoding

Documented encoding at lines 61-75:

```text
field_id   u16 LE
type       u8
flags      u8
value_len  u16 LE
value      value_len bytes
```

This is internally consistent:

- `field_id` is u16 LE; IDs are scoped by record type.
- `type` has u8/u16/u32/i32/f32/UTF-8 string cases.
- `flags` is u8; bit0 is optional-present; schema fields use 0.
- `value_len` is u16 LE; field size `6 + width`; strings carry UTF-8 bytes with no NUL.
- Unknown fields are skipped by `value_len`; a truncated field makes the record invalid.
- Strings are limited to 4096 bytes and invalid UTF-8 is rejected by contract.

No width/semantics contradiction was found in the table.

## 4. Minimal FULL arithmetic

Independent calculation:

```text
Envelope                       24
MAP payload: map_id TLV         6 + 4 = 10
MAP record: header 12 + 10      22
SELF payload:
  name 6+1                      7
  x 6+4                        10
  z 6+4                        10
  visual_model 6+1              7
  dir_deg 6+4                  10
  level u16 6+2                 8
  dead u8 6+1                   7
  SELF payload total           59 (0x3B)
SELF record: 12 + 59           71
Total: 24 + 22 + 71 = 117
Envelope payload_bytes: 22 + 71 = 93 (0x5D)
```

The document's example values `payload_bytes=0x5D` and total `117` are correct. The byte offsets 24 (MAP) and 46 (SELF) also add correctly.

## 5. FULL/DELTA and field semantics

- Entity IDs are consistent: MAP/EVENT/HUD use 0; SELF uses player uid; ACTOR_UPSERT/ACTOR_REMOVE use actor id.
- FULL requires one MAP and one SELF; omitted actors are not live after baseline.
- DELTA ACTOR_UPSERT is a full actor replacement; ACTOR_REMOVE deletes by header entity_id; EVENT appends/dedupes by event_id; HUD is an independent patch.
- `hp_known=0` is explicitly the unknown/suppress path; `hp=0` remains a valid HP value. This matches the old renderer's `hpKnown===false` and `hp>0` display behavior.
- Coordinates are f32 with no quantization; `dir_deg`/`guide_direction` are f32 degrees, with the old `PI/180` conversion cited at `view51.js:304-306` and `:636`.
- `ACTOR_REMOVE` has no payload fields; this is correctly excluded from the 65 count.
- TODO fields (MAP instance/country/scene, SELF HP/inventory, actor y/appearance, HUD buffs/channel/food/gather/ladder, event presence rules, etc.) are explicitly listed under "Unknown / not frozen"; they are not presented as frozen IDs.

No same-field different-width conflict, entity-id contradiction, deletion-semantics contradiction, `hp_known` contradiction, or coordinate-precision contradiction was found.

## 6. Test-only boundary

`0x7F01` is explicitly labeled `TEST_ONLY` and "test-only; replace, never production" in the document (`ZG-BINARY-RECORD-SCHEMA.md:4-5, 57, 315, 321-322`). Its C constant is `ZG_TEST_RECORD_TYPE` and the source comment says "Test-only record; replace with the PC contract record later."

**CONCERN:** the current test APK still emits and the JS adapter still logs `type=7F01`; this is acceptable only as a Phase-3 test transport. It must not be represented as a production/game record. The schema itself does not confuse it with a game field.

## 7. Findings summary

| Item | Result |
|---|---|
| Document SHA identity | PASS |
| Exactly 65 field definitions | PASS |
| Every field has old-JS source reference | PASS: 65/65 |
| Referenced lines exist in source | PASS: 82/82 |
| MAP/SELF/ACTOR/EVENT/HUD samples | PASS |
| TLV widths/UTF-8/unknown skip | PASS |
| 117-byte FULL example | PASS: 24+22+71=117; payload93 |
| Field width conflicts | PASS: none |
| entity/FULL/DELTA/hp_known/precision contradictions | PASS: none found |
| TODO fields not misrepresented as frozen | PASS |
| 0x7F01 test-only labeling | PASS in document/source |
| 0x7F01 current test/release presence | CONCERN: test-only only, must not ship as production |

Overall: **PASS for the schema freeze and its arithmetic/source traceability; non-blocking CONCERN that the current test build still carries 0x7F01 and must be replaced before production.** No product source, document, Java/JS or assets were modified; no push was performed.
