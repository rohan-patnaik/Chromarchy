# Raster source-preservation fixture provenance

`raster-source-preservation.json` embeds an original 124-byte PNG assembled
independently with the public PNG signature and IHDR, tEXt, IDAT, and IEND
chunk framing. Chunk CRC-32 values and the zlib-compressed scanlines were
generated with Python's standard library. It is a 3 by 2, 8-bit RGBA image with
opaque, partial-alpha, low-alpha, and transparent samples plus an `Author`
text field. It contains no third-party visual asset or private behavior.

The JSON fixes the complete source SHA-256. The workflow test writes those
bytes, imports them, edits the in-memory document, exports to a distinct PNG,
and verifies the source remains byte-identical. A separate writer-failure test
uses an unsupported extension after `QSaveFile` has opened its temporary output
and verifies cancellation preserves an existing destination byte-for-byte.
