# Native RGBA8 compatibility fixture provenance

`native-rgba8-baseline.json` contains fixed base64-encoded native v1 and v2
documents produced and loaded successfully with the unmodified codec at
Chromarchy revision `341ebfea38aa9e50fd62c57ebd62f0ba95216ae4` using Qt 6.11.2.
The source document is 4 by 2 pixels, has one layer named `Baseline RGBA8`, and
stores four nontransparent pixels covering alpha 1, 128, 254, and 255. The v1
fixture was made from that baseline writer's v2 output by changing only the
little-endian version field from 2 to 1 and removing the empty five-byte v2
selection trailer; the baseline reader loaded both files successfully.

The JSON records SHA-256 digests for both source fixtures and the first four
packed premultiplied RGBA8 pixels after decompression. Tests verify those fixed
bytes before exercising the current reader, writer, and reopen paths.

The hostile v2 fixture is derived from the fixed baseline v2 file by changing
the first packed pixel from valid `[1, 1, 0, 1]` to invalid `[2, 1, 0, 1]` and
recompressing only that tile. Its digest is fixed in the JSON. It is not a valid
native document and exists solely to prove that malformed premultiplied samples
are rejected without modifying the input file.

The legacy-zero v2 fixture was generated independently from the public v2 wire
contract using Qt 6.11.2. It is a 4 by 2 document with one visible layer named
`Legacy Zero Tile` and one explicitly stored, compressed all-byte-zero RGBA8
tile. Its SHA-256 digest is fixed in the JSON. Tests prove the current reader
accepts the legacy representation without modifying its source, canonicalizes
the resident layer to zero allocated tiles, and writes and reopens an equivalent
v2 document whose first layer declares zero tile records.

The legacy-base-selection v2 fixture was generated independently from the
public v2 wire contract using Qt 6.11.2. It is a 4 by 2 document with one empty
pixel layer, selection base coverage 255, and one explicitly stored grayscale
selection tile whose 65,536 samples are also 255. Its digest is fixed in the
JSON. Tests prove the declared record is fully parsed and charged before the
reader canonicalizes it to the absent base representation, leaves the source
unchanged, and writes and reopens an equivalent v2 document with zero selection
tile records.
