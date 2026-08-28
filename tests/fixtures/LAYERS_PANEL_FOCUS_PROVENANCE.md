# Layers-panel focus fixture provenance

`layers-panel-focus-contract.json` is an original Chromarchy interaction
contract authored for this repository. It is not copied from another editor or
external product.

The shortcuts are repository-local choices that do not conflict with existing
Chromarchy commands. F6 and Shift+F6 follow the conventional forward/reverse
workspace-area focus pattern while remaining an original two-area contract for
Chromarchy's current canvas and Layers panel. The 300,000-pixel dimension
reuses Chromarchy's public sparse-canvas limit, while 1,025 visibility and
focus changes cross a power-of-two boundary. The 100-millisecond ceilings are
the catalog's existing panel-interaction budget and cover synchronous offscreen
visibility/focus dispatch without rendering.
