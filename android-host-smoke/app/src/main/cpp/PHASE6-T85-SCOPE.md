# T85 Frozen Scope (pre-run)

## Scope definition

```text
MICRO-SCOPE map=xianyangcheng spawn=138,184 duration=30s paths=87 sha256=4a3a5015
```

- Zone: 新开四区
- Character: 烈火教父
- Spawn: 咸阳/听泉寨, UI coordinates `138,184`
- Fixed: no movement, no map change, no zoom
- Wait 30 seconds after scene entry, then freeze FINAL
- Scope path set = actual cold-run requested resources from the current
  xianyangcheng spawn, not all 503 map chunks and not "all native107 textures"

## Scope set

`paths=87`: 54 `native107/textures/*.png`, 32
`scenes/xianyangcheng/*.bin`, plus `scenes/xianyangcheng/map.json`.

Scope SHA-256 (sorted path list, newline-terminated):

```text
4a3a5015e78e4000602470889a7ad77a7e8185dc18ab5dcf2eb8fe394b7f70b4
```

Full list:

```text
native107/textures/16777239.png
native107/textures/16777245.png
native107/textures/16777305.png
native107/textures/16777309.png
native107/textures/16777313.png
native107/textures/16777314.png
native107/textures/16777315.png
native107/textures/16777334.png
native107/textures/16777335.png
native107/textures/16777421.png
native107/textures/16777464.png
native107/textures/16777502.png
native107/textures/16777582.png
native107/textures/16777605.png
native107/textures/16777613.png
native107/textures/16777648.png
native107/textures/16777666.png
native107/textures/16777667.png
native107/textures/16777683.png
native107/textures/16777684.png
native107/textures/16777685.png
native107/textures/16777686.png
native107/textures/16777735.png
native107/textures/16777736.png
native107/textures/16777743.png
native107/textures/16777744.png
native107/textures/16777748.png
native107/textures/16777749.png
native107/textures/16777750.png
native107/textures/16777751.png
native107/textures/16777752.png
native107/textures/16777753.png
native107/textures/16777772.png
native107/textures/16777773.png
native107/textures/16777774.png
native107/textures/16777775.png
native107/textures/16777780.png
native107/textures/16777783.png
native107/textures/16777784.png
native107/textures/16777787.png
native107/textures/16777788.png
native107/textures/16777789.png
native107/textures/16785272.png
native107/textures/16786042.png
native107/textures/16786043.png
native107/textures/16786069.png
native107/textures/16786070.png
native107/textures/16786071.png
native107/textures/16786072.png
native107/textures/16786117.png
native107/textures/16842329.png
native107/textures/16842339.png
native107/textures/16842349.png
native107/textures/16842369.png
scenes/xianyangcheng/10_11.bin
scenes/xianyangcheng/10_12.bin
scenes/xianyangcheng/10_13.bin
scenes/xianyangcheng/10_14.bin
scenes/xianyangcheng/10_4.bin
scenes/xianyangcheng/10_5.bin
scenes/xianyangcheng/10_6.bin
scenes/xianyangcheng/10_7.bin
scenes/xianyangcheng/11_11.bin
scenes/xianyangcheng/11_12.bin
scenes/xianyangcheng/11_13.bin
scenes/xianyangcheng/11_14.bin
scenes/xianyangcheng/12_11.bin
scenes/xianyangcheng/12_12.bin
scenes/xianyangcheng/12_13.bin
scenes/xianyangcheng/12_14.bin
scenes/xianyangcheng/13_11.bin
scenes/xianyangcheng/13_12.bin
scenes/xianyangcheng/13_13.bin
scenes/xianyangcheng/13_14.bin
scenes/xianyangcheng/7_4.bin
scenes/xianyangcheng/7_5.bin
scenes/xianyangcheng/7_6.bin
scenes/xianyangcheng/7_7.bin
scenes/xianyangcheng/8_4.bin
scenes/xianyangcheng/8_5.bin
scenes/xianyangcheng/8_6.bin
scenes/xianyangcheng/8_7.bin
scenes/xianyangcheng/9_4.bin
scenes/xianyangcheng/9_5.bin
scenes/xianyangcheng/9_6.bin
scenes/xianyangcheng/9_7.bin
scenes/xianyangcheng/map.json
```

## Per-path status policy

For each scope path the next run must emit:

```text
MICRO-REQ path=<path> required=1 status=requested|ok|retry|fail attempts=<n>
```

Only paths actually requested by the fixed camera/spawn may appear with
`referenced=0` or no `MICRO-REQ`; they must not be reported as successful just
because aggregate `requiredFail=0`.

`11_13.bin` appears only with its actual run status; if it does not retry, it
must be `attempts=1 ok`. Optional sprites are reported separately.
