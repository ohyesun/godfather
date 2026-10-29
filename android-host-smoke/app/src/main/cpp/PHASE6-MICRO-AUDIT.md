# ZGMU 18008 Micro-Resource Audit (T77)

## Conclusion

The host **has no ZGMU/18008 client and no `path:` GET layer**. The asset path
handler only serves:

1. `AssetManager.open(path)`
2. local `resources107-index.json` + `resources107.pak`

For index entries with `offset < 0` / `length < 0`, `PackedAssetsPathHandler`
logs `pak miss negative` and returns `null`. There is no fallback to TCP 18008,
no `PakRemote`/`PakFetcher`, and no HELLO/GET implementation in the host Java
sources.

Therefore the current “loading map” state has **two independent possible
causes**:

- 6020 scene frames not progressing (T76), and
- the micro-resource layer being absent for the 80,457 negative-offset entries.

They must not be conflated.

## Source audit

Host Java classes:

```text
DirectTcpBridge.java
Game6020ClientMonitor.java
Game6020Monitor.java
Login7000Monitor.java
MainActivity.java
PackedAssetsPathHandler.java
TlsTransport.java
ZgBridge.java
```

No `Zgmu.java`, `PakRemote.java`, `PakFetcher.java`, or any 18008 socket code.
The only `micro` token in `client.js` is the protocol codec empty-frame counter
(`stats.micro++` for length-2 `00 00` frames), not a micro-resource client.

Assets present:

- `micro-client.json` (host/port config only; not consumed)
- `resources107.pak` (29 MB)
- `resources107-index.json` (82,344 entries)

Index statistics:

```text
total entries: 82,344
offset < 0 / length < 0: 80,457
local/positive entries: 1,887
```

Example negative entries include `action-audio107.js`, audio/model resources;
these are exactly the resources the micro client should fetch by
`path:<path>`.

## Runtime audit

While the host was running at the post-login/loading state, `/proc/net/tcp`
and `/proc/net/tcp6` were queried for remote port 18008 (`:4658`); no matching
socket was present. This is consistent with the source audit: no 18008
connection or GET is initiated by the host.

No token/key/ticket content was printed. The config asset exists but its token
is not logged here.

## Minimal port scope (recommended)

If proceeding, the smallest compatible implementation is:

1. `Zgmu.java`: ZGMU/1 HELLO (0x01/0x81), GET (0x10/0x90), END flag 0x02,
   `path:<path>`, offset=-1 semantics, bounded reconnect/retry.
2. `PakRemote.java` equivalent: only `openRemote` for negative-offset index
   entries; local cache hit first; path SHA-based cache name; no remote JS/CSS/
   SVG/HTML.
3. `PackedAssetsPathHandler`: on `offset < 0 || length < 0`, call remote layer
   instead of returning `null`; preserve local positive-offset behavior.
4. Config from `micro-client.json`; do not log token or payload; log only
   `MICRO-OK path=... bytes=...` / `MICRO-FAIL code=...`.
5. Keep 6020 and micro-resource failure diagnostics separate.

No source changes made in this audit.
