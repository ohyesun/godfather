# MICRO 18008 Failed-Path Triage (T79)

## Scope

31 unique paths from the T78 single-run `MICRO-FAIL` set (33 failure events,
two paths retried). Every path was probed directly against the real 18008
server using the same ZGMU client and the index `storedLength`.

## Result

**All 31 direct GETs returned `OK` with exact byte lengths.** No
`E_NO_PACK`, `E_OFFSET`, `E_IO`, or `E_PROTO` was returned by 18008. All index
entries exist with `offset=-1` and positive stored lengths. Therefore:

- not a server-missing-file problem;
- not a client index offset/length problem;
- the `MICRO-FAIL code=io` in the app is a client-side transient fetch/cache/
  concurrency issue (retry/pool behavior), not a protocol payload error.

The 33 events vs 31 unique paths is consistent with retries/duplicate
in-flight requests; the direct probe returned complete bytes for every path.

## Triage table

| path | index `[offset,storedSize,codec]` | 18008 | required? | action |
|---|---|---|---|---|
| `native107/textures/16777577.png` | `[-1,40224,0]` | `OK 40224/40224` | required (map texture candidate) | client retry/pool; do not skip |
| `native107/textures/16786122.png` | `[-1,55229,0]` | `OK 55229/55229` | required (map texture candidate) | client retry/pool; do not skip |
| `scenes/sprites/12374.webp` | `[-1,14689,1]` | `OK 14689/14689` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/12427.webp` | `[-1,12220,1]` | `OK 12220/12220` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/13326.webp` | `[-1,518,0]` | `OK 518/518` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/13738.webp` | `[-1,28096,1]` | `OK 28096/28096` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/14185.webp` | `[-1,27445,1]` | `OK 27445/27445` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/14209.webp` | `[-1,264,0]` | `OK 264/264` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/14222.webp` | `[-1,17478,1]` | `OK 17478/17478` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/14411.webp` | `[-1,542,0]` | `OK 542/542` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/14452.webp` | `[-1,3644,0]` | `OK 3644/3644` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/14485.webp` | `[-1,37820,1]` | `OK 37820/37820` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/14505.webp` | `[-1,3988,0]` | `OK 3988/3988` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/14672.webp` | `[-1,36455,1]` | `OK 36455/36455` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/4569.webp` | `[-1,16148,1]` | `OK 16148/16148` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/7432.webp` | `[-1,22671,1]` | `OK 22671/22671` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/7457.webp` | `[-1,16569,1]` | `OK 16569/16569` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/7491.webp` | `[-1,22280,1]` | `OK 22280/22280` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/7497.webp` | `[-1,44457,1]` | `OK 44457/44457` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/8034.webp` | `[-1,226,0]` | `OK 226/226` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/8249.webp` | `[-1,103100,1]` | `OK 103100/103100` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/8252.webp` | `[-1,16863,1]` | `OK 16863/16863` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/8346.webp` | `[-1,14520,1]` | `OK 14520/14520` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/8461.webp` | `[-1,21275,1]` | `OK 21275/21275` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/8872.webp` | `[-1,21669,1]` | `OK 21669/21669` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/9075.webp` | `[-1,8497,1]` | `OK 8497/8497` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/9720.webp` | `[-1,34710,1]` | `OK 34710/34710` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/9721.webp` | `[-1,41158,1]` | `OK 41158/41158` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/sprites/9885.webp` | `[-1,30868,1]` | `OK 30868/30868` | optional sprite candidate | keep failure log; no fake asset |
| `scenes/xianyangcheng/12_14.bin` | `[-1,2363,1]` | `OK 2363/2363` | required (map block) | client retry/pool; do not skip |
| `scenes/xianyangcheng/13_13.bin` | `[-1,2483,1]` | `OK 2483/2483` | required (map block) | client retry/pool; do not skip |

## Disposition

- No server file is missing; do not hardcode/synthesize any asset.
- No index offset/length mismatch; `PakRemote` GET length is correct.
- The immediate client fix is bounded retry/reuse of ZGMU connections for
  transient `io` failures, especially for the four required paths above.
- Optional sprites remain non-fatal; their failures stay visible in
  `MICRO-FAIL` and must not be silently replaced.
- T78 count CONCERN remains until a single-run `MICRO-RUN` shows the required
  paths reliably loaded; this triage changes the cause from “server missing”
  to “client transient fetch.”
