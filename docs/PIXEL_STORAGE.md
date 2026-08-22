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

## Typed tile storage

`PixelTile` owns one packed 256-square tile with a hard one-MiB ceiling, enough
for four 32-bit channels. Allocation is zero-initialized and checked against
both the fixed ceiling and a caller limit. Unsigned 16-bit, 16-bit float, and
32-bit float tiles preserve explicit format and byte-order descriptors and
expose only exact per-pixel byte spans; this slice does not interpret their
numeric values or assign color meaning beyond the channel descriptor.

Tile copies share their byte allocation until a mutation, and setters reject
out-of-range coordinates, wrong sample sizes, identical no-ops, and invalid
premultiplied RGBA8 samples. `isZero()` is the explicit seam for a future sparse
typed-tile owner to elide empty allocations. High-depth premultiplied storage is
rejected until its integer/float validity policy is defined.

`fromPackedBytes()` requires the exact checked payload and retains the caller's
format descriptor, providing a bounded byte-preserving persistence seam without
changing the native document format. `toRgba8Premultiplied()` succeeds only for
the existing RGBA8 formats. High-depth conversion deliberately reports
unsupported until numeric scaling, rounding, nonfinite, and display policies
are specified and reviewed.

## Current engine boundary

The live sparse `TiledImage` engine still stores only RGBA8 premultiplied
`QImage` tiles. QImage adapters accept only `Format_RGBA8888` or
`Format_RGBA8888_Premultiplied` and always return the existing premultiplied
live format. Native v1/v2 tile encoding and decoding use these adapters, but the
native version and packed premultiplied RGBA8 wire bytes are unchanged. Fixture
tests prove byte-identical v2 save/reopen and pixel-identical v1 load, upgrade,
and reopen across tile boundaries.

High-depth tiles are an isolated storage prerequisite: documents and the live
render path do not own or render them yet. There is no high-depth numeric
conversion, native persistence, import/export round trip, or color-management
claim. Import, export, display, and render-node boundaries beyond the native
RGBA8 tile adapter remain future work.

RGBA8 pixel clears also reclaim a tile once its complete payload becomes zero.
The check runs only after a transparent write and never removes a tile that
still contains another nonzero pixel. Import already omits fully transparent
tiles. Broader normalization of tiles produced by merge, flatten, or native-load
paths remains future work.

This slice adds no color interpretation, profiles, transfer functions, or
color-library dependency.
