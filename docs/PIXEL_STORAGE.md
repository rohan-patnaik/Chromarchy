# Pixel storage contracts

Phase 1 starts with a dependency-free description of pixel memory. The
`PixelFormat` value records all information needed to interpret one pixel:

- unsigned 8-bit, unsigned 16-bit, IEEE-style 16-bit float, or 32-bit float
  samples;
- gray, gray-plus-alpha, RGB, or RGBA channel layout, with a defined meaning
  for every channel index;
- no alpha, straight alpha, or premultiplied alpha; and
- explicit little- or big-endian order for multi-byte samples. Byte order is
  deliberately not applicable to one-byte samples.

Invalid combinations are rejected. An alpha-bearing layout must declare
straight or premultiplied alpha, a layout without alpha must declare none, and
every multi-byte format must choose a byte order.

`PixelStorageLayout::create` is the checked boundary for dimensions and byte
geometry. It rejects non-positive dimensions, invalid formats, non-power-of-two
row alignment, row/alignment/total-size overflow, totals that do not fit
`qsizetype`, and caller-supplied allocation limits. The same calculation covers
full buffers, scanline-backed adapters, and individual tiles; the checked
RGBA8 256-square tile size is 262,144 bytes.

## Current engine boundary

The live sparse `TiledImage` engine still stores only RGBA8 premultiplied
`QImage` tiles. It now exposes that format and its checked tile layout rather
than leaving the contract implicit. The new high-depth values are descriptions
and validation prerequisites only: they do not claim high-depth allocation,
rendering, conversion, native persistence, or format round trips.

RGBA8 pixel clears also reclaim a tile once its complete payload becomes zero.
The check runs only after a transparent write and never removes a tile that
still contains another nonzero pixel. Import already omits fully transparent
tiles. Broader normalization of tiles produced by merge, flatten, or native-load
paths remains future work.

Conversions must be explicit at import, export, display, and future render-node
boundaries. This slice adds no color interpretation, profiles, transfer
functions, or color-library dependency.
