# Layers-panel focus fixture provenance

`layers-panel-focus-contract.json` is an original Chromarchy interaction
contract authored for this repository. It is not copied from another editor or
external product.

The shortcuts are repository-local choices that do not conflict with existing
Chromarchy commands. The 300,000-pixel dimension reuses Chromarchy's public
sparse-canvas limit, while 1,025 visibility changes cross a power-of-two
boundary. The 100-millisecond ceiling is the catalog's existing panel-toggle
budget and covers synchronous offscreen visibility dispatch without rendering.
