# Document-tab navigation fixture provenance

`document-tab-navigation-contract.json` is an original Chromarchy interaction
contract authored for this repository. It is not copied from another editor or
external product.

The shortcuts use conventional local desktop document navigation keys. The
three labels are synthetic and exist only to make a post-reorder visual order
unambiguous. The 300,000-pixel dimension reuses Chromarchy's public sparse
canvas limit, while 1,025 repeated switches crosses a power-of-two boundary.
The 50-millisecond per-switch ceiling is the existing catalog budget and
excludes document rendering; the offscreen fixture measures synchronous
navigation dispatch over already open, empty sparse documents.
