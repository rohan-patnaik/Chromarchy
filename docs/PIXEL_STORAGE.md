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

`PixelStorageLayout::createWithRowStride` validates externally supplied
scanlines. It rejects a stride smaller than the packed row, total-size overflow,
and caller allocation limits. RGBA8 conversion requires the source span to
match that checked total exactly, so truncated and trailing payloads fail before
pixel access. Destination padding is initialized to zero.

The dependency-free RGBA8 adapters currently support only explicit straight
and premultiplied RGBA8. Straight-to-premultiplied conversion rounds
`channel * alpha / 255` to the nearest integer. The inverse uses the matching
bounded integer calculation; transparent premultiplied samples decode to zero,
so hidden straight color at alpha zero is intentionally not recoverable. The
checked-in byte vectors cover alpha 0, 1, 127, 128, 254, and 255 independently
of Qt image conversion.

Premultiplied input is strict: every stored color channel must be less than or
equal to alpha. This includes requiring zero color channels when alpha is zero.
Invalid samples are rejected before destination allocation or copying rather
than being clamped into the live engine. Row and pixel byte offsets use checked
layout values and unsigned arithmetic throughout conversion.

## Current engine boundary

The live sparse `TiledImage` engine still stores only RGBA8 premultiplied
`QImage` tiles. QImage adapters accept only `Format_RGBA8888` or
`Format_RGBA8888_Premultiplied` and always return the existing premultiplied
live format. Native v1/v2 tile encoding and decoding use these adapters, but the
native version and packed premultiplied RGBA8 wire bytes are unchanged. Fixture
tests prove byte-identical v2 save/reopen and pixel-identical v1 load, upgrade,
and reopen across tile boundaries.

The high-depth values remain descriptions and validation prerequisites only:
they do not claim high-depth allocation, rendering, conversion, native
persistence, or format round trips. Import, export, display, and render-node
boundaries beyond the native RGBA8 tile adapter also remain future work.

RGBA8 pixel clears also reclaim a tile once its complete payload becomes zero.
The check runs only after a transparent write and never removes a tile that
still contains another nonzero pixel. Import already omits fully transparent
tiles. Broader normalization of tiles produced by merge, flatten, or native-load
paths remains future work.

This slice adds no color interpretation, profiles, transfer functions, or
color-library dependency.
