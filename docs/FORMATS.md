# Raster format and metadata policy

Chromarchy uses the image codecs supplied by the installed Qt 6 image-format plugins. The Arch package depends on `qt6-base` and `qt6-imageformats`, which provide the target installation's PNG, JPEG, TIFF, WebP, and OpenEXR adapters where supported by that Qt build.

## Import

- Decoder orientation is applied before pixels enter the document.
- Imported pixels become a real tiled pixel layer; the source file is never modified.
- Embedded textual metadata is not copied into the working document in the current M1 model.
- Images with invalid, zero, or over-300,000-pixel dimensions are rejected before decode.
- Application-owned guards reject source files above 512 MiB and declared images
  above 64 MiPixels or 256 MiB of RGBA decoded pixels before calling the Qt
  decoder. These are aggregate per-image bounds, not codec-plugin guarantees.
- Raster decode is currently synchronous and in-process. Per-codec wall-time,
  cancellation, and helper-process isolation require the approved job/helper
  architecture and remain incomplete; hostile inputs should not be treated as
  fully isolated yet.

## Export

- Export composites through the deterministic CPU path and writes through `QSaveFile` for atomic replacement.
- PNG, TIFF, WebP, and OpenEXR preserve alpha when the installed codec supports it.
- JPEG has no alpha channel; transparent areas are explicitly composited over white.
- Metadata is stripped by default. This avoids accidentally carrying author, location, device, or workflow data into a derivative file.
- Full-frame export is currently limited to 64 MiPixels (256 MiB of RGBA pixels before encoder overhead). Oversized documents fail before allocation with an actionable resize/crop message; tile-streaming encoders are future work.

Metadata inspection and selective preservation controls are planned. Until those controls exist, users who require source metadata must retain the original file separately.

## Cataloged format decisions

Codec availability is not a compatibility claim. BMP, still GIF, TGA, and the
PBM/PGM/PPM family are Planned until installed-codec audits, hostile fixtures,
public round trips, metadata/profile behavior, and the application limits above
are evidenced. Animated GIF and APNG are separately tracked media workflows.
JPEG 2000 is Blocked on an independently licensed codec, security/isolation,
and packaging decision.

PDF artwork interchange is distinct from PDF proof export and is Blocked on a
PDF adapter, license, process-isolation, and vector/raster-loss policy decision.
EPS interchange is Blocked on an even stricter PostScript interpreter, license,
network-off sandbox, and resource-limit decision. No PDF, EPS, JPEG 2000, or
other consequential dependency has been adopted by cataloging the workflow.

## Native document bounds

Native loads and saves reject files above 64 MiB, more than 64 combined
pixel/selection tiles, more than 32 MiB of aggregate compressed tile payload,
or more than 16 MiB of aggregate decoded tile payload. Save checks are applied
before atomic publication, and any failure preserves an existing destination.
Per-tile compressed and decompressed
sizes, layer count, name length, coordinates, duplicates, and trailing data are
also checked before acceptance. Loading is currently synchronous and
in-process, so cancellation, wall-time enforcement, and helper isolation remain
explicitly incomplete.
