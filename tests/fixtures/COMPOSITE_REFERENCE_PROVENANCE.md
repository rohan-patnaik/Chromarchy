# Source-over composite golden provenance

`composite-source-over-golden.json` is an original 257 by 2 pixel fixture. It
contains no vendor sample, asset, UI, wording, or private behavior. Its four
small sparse layers exercise transparent and opaque endpoints, low and high
alpha, fractional layer opacity, hidden-layer exclusion, order, and the x=255
to x=256 tile boundary.

The expected full image was calculated independently with exact rational
Porter-Duff source-over arithmetic in straight RGBA. After each bottom-to-top
layer, effective source alpha is `sample alpha * layer opacity`; output alpha
is `source alpha + destination alpha * (1 - source alpha)`; unassociated color
channels are divided by output alpha and all four channels are rounded to the
nearest unsigned byte. Unspecified pixels are explicitly transparent. The
fixture records the SHA-256 of all 2,056 row-major RGBA bytes, so its sparse
notation still defines and authenticates the complete image.

The production renderer is permitted two bytes of compounded integer-raster
rounding difference per channel from this independent straight-alpha reference;
the bound covers repeated conversion through premultiplied 8-bit layer and
destination storage. Its own output must remain byte-identical across repeated
full renders, bounded region renders, the painter entry point, native
save/reopen, and flattening. A complete merge-down sequence remains within the
same independent bound, and its persisted result must reopen byte-identically.
