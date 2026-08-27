# Live pixel-tile storage decision packet

Status: owner decision required; no option is adopted by this document.

## Decision boundary

The live RGBA8 `TiledImage` has sparse copy-on-write payloads but no hard
resident tile or decoded-byte quota. Native v2 persistence separately limits a
file to 64 combined pixel/selection tiles and 16 MiB of decoded tile payload,
while raster import currently admits one image up to 64 MiPixels and 256 MiB of
decoded RGBA. Applying the native limit to live documents would therefore
reject inputs that the current import contract accepts.

`setPixelColor()` also returns one boolean for both unchanged and rejected
requests. Enforcing a new quota through that result could silently present an
out-of-memory refusal as a no-op. Quota scope, failure propagation, and import
admission are consequential storage and compatibility choices, so neither a
quota nor a mutation-result redesign is approved implicitly.

## Options

1. **Align live storage with native v2.** Enforce 64 tiles/16 MiB in memory.
   This is simple but breaks current large raster import and constrains layered
   documents to a monolithic file-format limit.
2. **Independent fixed live-document quota.** Preserve the current single-image
   import envelope with a larger per-document tile/byte ceiling. This needs an
   explicit multi-layer admission policy, atomic failures, and a decision on
   whether decoded import plus tiled conversion may temporarily coexist.
3. **Staged budget service.** First approve an explicit changed/unchanged/
   rejected mutation result and exact live accounting without enforcement;
   then choose configurable per-document and process ceilings with low-memory,
   import, history, cache, and cancellation policy. This has the cleanest
   failure contract but crosses the shared cache/job architecture boundary.

## Recommendation

Approve only the prerequisite part of option 3 first: an explicit mutation
result plus observable exact pixel-payload accounting, with no new quota and no
change to accepted imports or native persistence. Use measurements from that
contract to return a separate defaults/enforcement packet. Do not silently drop
pixels, reuse the native v2 ceiling as a live limit, or expose a configurable
control before its semantics are fixed.

## Owner choices required

- quota scope: layer, document, process, or a documented combination;
- hard defaults and whether users may configure them;
- raster-import preflight and temporary conversion working-set treatment;
- atomic error propagation through editing, undo/history, save, and UI;
- relationship, if any, between live limits and native v2 limits; and
- behavior for documents already resident when a limit is lowered.

Approval phrase for the recommended prerequisite:

> Approve explicit live pixel mutation results and exact payload accounting
> only; retain current import/native behavior and return for quota approval.
