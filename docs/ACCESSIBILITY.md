# Accessibility evidence and limits

Chromarchy exposes original accessible names and descriptions for the current
document workspace, document tabs, canvas, layers panel, layer list, layer
actions, opacity control, and pixel-lock control. The flat layers panel has an
explicit tab sequence from the layer list through opacity and lock controls.

`MainWindowTest` queries Qt's accessibility interfaces offscreen and verifies
that the core workspace objects have non-empty roles, stable names, and
descriptions. It also drives a pointer-free path through those layer controls
and exercises Select All, create layer, duplicate layer, undo, and redo by
keyboard. The redo test guards against duplicate platform bindings that Qt
would otherwise reject as ambiguous.

This is a partial accessibility slice, not a completed audit. Dialogs, every
menu and action state, nested or multi-selected layers, focus-trap traversal,
high-contrast and scale behavior, live AT-SPI event/state exposure, and a real
screen-reader workflow remain unverified. Packaged launch evidence may confirm
that the application starts without taking the active workspace, but it does
not replace live assistive-technology validation.
