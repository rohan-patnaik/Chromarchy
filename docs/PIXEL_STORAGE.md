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
changing the native document format. `convertUnsignedToRgba8Premultiplied()`
and the tile `toRgba8Premultiplied()` seam accept unsigned 8-bit and unsigned
16-bit Gray, GrayAlpha, RGB, and straight-alpha RGBA input. Gray is replicated
to RGB, absent alpha becomes opaque, and 16-bit samples honor the declared byte
order. Conversion maps the full unsigned range with nearest-integer rounding;
straight color is multiplied by alpha at source precision and rounded once into
the premultiplied 8-bit result. Destination geometry, row alignment, payload,
and allocation limits remain checked, and output padding is byte-zero.

Strict premultiplied RGBA8 remains accepted after validating color channels do
not exceed alpha. Higher-depth premultiplied and all floating-point conversion
remain rejected until their validity, nonfinite, range, transfer, and display
policies are reviewed. This numeric adapter is a deterministic conversion into
the existing RGBA8 working boundary, not a color-space or profile transform.

`SparsePixelTileStore` is an isolated typed owner over `PixelTile`. One store
has one immutable pixel format and positive logical dimensions. Pixel and tile
coordinates are checked against those dimensions with unsigned tile-grid
arithmetic, including maximum `QSize` dimensions. Missing tiles read as exact
byte-zero samples without allocating, and byte-zero writes to missing tiles are
no-ops.

Resident decoded payload is caller-bounded and hard-capped at 16 MiB and 64
tiles, matching the existing native decoded-payload and tile-count ceilings.
The per-format tile size is charged exactly before insertion; a format whose
single tile exceeds the caller budget is rejected when the store is created.
Wrong-size, invalid premultiplied RGBA8, out-of-range, tile-limit, and byte-limit
writes leave the store and its accounting unchanged. Clearing the last nonzero
byte elides the complete tile and releases its decoded-byte charge.

Store and tile copies retain Qt copy-on-write isolation. Raw high-depth sample
bytes and their declared endianness are preserved without numeric
interpretation. The decoded-byte counter covers packed tile payloads exactly;
`QHash`, allocator, and object metadata remain outside that counter, though the
hard tile-count ceiling bounds the number of resident entries.

`packedTileBytes()` is a borrowed read view. It is valid only until the store is
destroyed or any non-const operation is attempted on that store; callers that
need bytes across mutation must copy them first. Consecutive nonzero writes to
a uniquely owned tile retain its allocation. Mutating a copied store detaches
only the affected tile, and the complete byte-zero scan runs only after a
byte-zero sample write can make a tile empty.

## Owning snapshot boundary

`tileSnapshots()` returns owning records containing a tile index and its exact
packed bytes. Records are always exported in row-major index order, independent
of `QHash` iteration order. The source store may be destroyed or mutated after
export without changing a snapshot, and snapshot bytes may be mutated without
changing the store. Because resident payload is hard-bounded, one exported
snapshot set owns at most 16 MiB of packed tile bytes plus at most 64 records
and their container metadata.

`fromTileSnapshots()` builds a new store or returns no store. It accepts input
record order but rejects duplicate or out-of-grid indices, truncated or
trailing payload bytes, invalid premultiplied RGBA8 samples, aggregate byte or
tile limits, and all-byte-zero resident records. Rejecting zero records keeps
the sparse representation canonical; absent tiles already represent those
bytes. Import copies exact bytes, preserves the declared sample endianness, and
does not modify its source records. A rejected input exposes no partially built
store.

These records are an in-memory persistence boundary, not a serialized format.
They add no headers, framing, version, native v1/v2 change, or raster/document
integration. Any future on-disk container requires its own reviewed format and
compatibility contract.

## Bounded rectangular region boundary

`readRegion()` and `writeRegion()` move exact packed sample bytes across a
rectangular part of `SparsePixelTileStore`, including regions that cross sparse
tile boundaries. They do not interpret or convert numeric samples. Reads return
an owning buffer with a checked layout; absent pixels and row padding are
byte-zero, and the store's declared sample byte order is unchanged.

Regions must be nonempty and fully inside the logical image. Checked unsigned
geometry rejects coordinate, stride, row-size, and total-size overflow. One
operation may touch at most 64 tiles. A read allocation is caller-bounded and
hard-capped at 16 MiB. A write requires an exact source span and explicit row
stride. Its caller limit independently bounds both the staged source allocation
and the aggregate full-tile replacement payload, each with a 16-MiB hard cap;
the two temporary payload sets can coexist during validation, in addition to
the existing store and bounded container metadata.

Writes stage the complete source before inspecting or changing the store, so a
source may overlap a resident tile payload without invalidation or undefined
copy behavior. All touched replacements, premultiplied RGBA8 samples, and final
resident budgets are validated before a candidate store is committed. A
failure exposes no partial mutation. Byte-identical writes return `Unchanged`,
successful mutations return `Changed`, and failures return `Rejected`; these
are dirty-state-equivalent results rather than a document dirty-state tracker.
All-zero replacements are elided, and copying then mutating a store preserves
the original through tile-level copy-on-write isolation.

The returned read buffer is owning. As with other non-const store operations,
attempting a region write invalidates previously borrowed `packedTileBytes()`
views even when it returns `Unchanged` or `Rejected`; callers needing stable
input must own it or rely on the write's internal staging during that call.
This boundary adds no native or raster persistence, document/render wiring,
implicit numeric conversion, scaling, nonfinite policy, or color management.

`readRgba8PremultipliedRegion()` is the explicit deterministic materialization
seam from that sparse owner into the existing RGBA8 premultiplied boundary. It
supports the same unsigned Gray, GrayAlpha, RGB, straight RGBA, and strict
premultiplied RGBA8 formats as the checked numeric adapter. Missing samples keep
their declared numeric meaning: an absent alpha-bearing pixel is transparent,
while an absent Gray or RGB pixel without alpha becomes opaque black.

The source region is first packed without row padding, then converted with the
requested destination alignment. One caller limit hard-capped at 16 MiB covers
the sum of both simultaneously live owning buffers; overflow, an invalid
alignment or region, more than 64 touched tiles, unsupported float/high-depth
premultiplication, or either buffer exceeding the combined cap is rejected
before allocation. The store is not mutated and repeated reads, including from
a snapshot-rebuilt equivalent store, produce identical bytes. This seam does
not add scaling, caching, cancellation, a render node, document integration, or
floating-point policy.

## Reversible typed-tile delta boundary

`tileDeltaTo()` compares stores only when their logical dimensions and pixel
formats match. Its owning delta envelope retains those exact dimensions and the
complete `PixelFormat`, then emits changed-tile records in strict row-major
order, independent of `QHash` order. Each record carries an optional before tile
and optional after tile; absence means the canonical byte-zero sparse tile.
Records whose two sides are equal are omitted. Exact packed bytes and declared
sample endianness are preserved without numeric interpretation.

One delta is caller-bounded and hard-capped at 64 records and 16 MiB of combined
before-plus-after packed payload. Generation returns no delta if either cap is
exceeded. The returned records are owning Qt byte arrays and can share immutable
payload with their source stores until either owner mutates; changing a returned
record cannot change either source store.

`applyTileDelta()` first requires the envelope dimensions and complete format,
including sample kind, channels, alpha mode, and byte order, to equal the target
store. It then accepts only the same canonical row-major record order. It
rejects duplicate or out-of-grid indices, absent-to-absent or equal-sided
records, truncated/trailing payloads, resident all-zero payloads, invalid
premultiplied RGBA8, unknown directions, and caller or hard-cap violations. The
selected before or after side must exactly match every current resident tile.
The envelope binding and byte conflict check together prevent application to a
wrong typed state. Both sides of every record, the final resident tile/byte
budgets, and all conflicts are validated before a candidate store is committed,
so forward and reverse application expose no partial mutation. An empty
canonical delta is an `Unchanged` no-op; a nonempty valid application is
`Changed`; every invalid, conflicting, or over-budget input is `Rejected`.

Delta records are an in-memory reversible raw-tile prerequisite, not a command,
serialized encoding, native-format addition, dirty-state tracker, or coalescing
policy. They are not integrated with live `DocumentHistory`, which continues to
retain copy-on-write RGBA8 document snapshots. Consequently the cataloged
reversible delta/coalesced history capability remains Planned.

## Current engine boundary

The live sparse `TiledImage` engine still stores only RGBA8 premultiplied
`QImage` tiles. QImage adapters accept only `Format_RGBA8888` or
`Format_RGBA8888_Premultiplied` and always return the existing premultiplied
live format. Native v1/v2 tile encoding and decoding use these adapters, but the
native version and packed premultiplied RGBA8 wire bytes are unchanged. Fixture
tests prove byte-identical v2 save/reopen and pixel-identical v1 load, upgrade,
and reopen across tile boundaries.

High-depth tiles, their sparse owner, snapshots, rectangular access, reversible
tile deltas, and unsigned-to-RGBA8 adapter are isolated storage prerequisites:
documents and the live render path do not own or render typed tiles yet. There
is no floating-point conversion, native typed persistence, high-depth
import/export round trip, or color-management claim. Import, export, display,
and render-node wiring beyond the native RGBA8 tile adapter remain future work.

RGBA8 pixel clears also reclaim a tile once its complete payload becomes zero.
The check runs only after a transparent write and never removes a tile that
still contains another nonzero pixel. Import already omits fully transparent
tiles. Merge, flatten, and native v1/v2 load now also omit byte-zero pixel
tiles, so those live document boundaries retain the same canonical sparse
representation. Declared native entries still count against all aggregate
input budgets before decoding, and this changes neither the v1/v2 wire format
nor selection-mask storage.

This slice adds no profile or transfer-function interpretation and no
color-library dependency.
