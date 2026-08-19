# Raster format and metadata policy

Chromarchy uses the image codecs supplied by the installed Qt 6 image-format plugins. The Arch package depends on `qt6-base` and `qt6-imageformats`, which provide the target installation's PNG, JPEG, TIFF, WebP, and OpenEXR adapters where supported by that Qt build.

## Import

- Decoder orientation is applied before pixels enter the document.
- Imported pixels become a real tiled pixel layer; the source file is never modified.
- Embedded textual metadata is not copied into the working document in the current M1 model.
- Images with invalid, zero, or over-300,000-pixel dimensions are rejected before decode.

## Export

- Export composites through the deterministic CPU path and writes through `QSaveFile` for atomic replacement.
- PNG, TIFF, WebP, and OpenEXR preserve alpha when the installed codec supports it.
- JPEG has no alpha channel; transparent areas are explicitly composited over white.
- Metadata is stripped by default. This avoids accidentally carrying author, location, device, or workflow data into a derivative file.
- Full-frame export is currently limited to 64 MiPixels (256 MiB of RGBA pixels before encoder overhead). Oversized documents fail before allocation with an actionable resize/crop message; tile-streaming encoders are future work.

Metadata inspection and selective preservation controls are planned. Until those controls exist, users who require source metadata must retain the original file separately.
