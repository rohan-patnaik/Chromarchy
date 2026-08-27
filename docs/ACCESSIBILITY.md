# Accessibility evidence and limits

Chromarchy exposes original accessible names and descriptions for the current
document workspace, document tabs, canvas, layers panel, layer list, layer
actions, opacity control, and pixel-lock control. The flat layers panel has an
explicit tab sequence from the layer list through opacity and lock controls.
The New Document dialog exposes names and descriptions for its bounded width,
height, and action controls, with explicit dimension focus order.

`MainWindowTest` queries Qt's accessibility interfaces offscreen and verifies
that the core workspace objects have non-empty roles, stable names, and
descriptions. It also drives a pointer-free path through those layer controls
and exercises Select All, create layer, duplicate layer, undo, and redo by
keyboard. The redo test guards against duplicate platform bindings that Qt
would otherwise reject as ambiguous. It also opens the New Document dialog by
shortcut, traverses and enters bounded dimensions, creates the document with
Enter, and verifies that Escape cancels without adding another document.

This is a partial accessibility slice, not a completed audit. File chooser,
save/export, unsaved-changes, and error dialogs; every menu and action state;
nested or multi-selected layers; complete focus-trap traversal; high-contrast
and scale behavior; live AT-SPI event/state exposure; and a real screen-reader
workflow remain unverified. Packaged launch evidence may confirm that the
application starts without taking the active workspace, but it does not replace
live assistive-technology validation.
