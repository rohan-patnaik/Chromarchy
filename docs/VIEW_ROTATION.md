# Non-destructive view rotation

Chromarchy can rotate the current canvas view in exact 90-degree increments
without rotating, resampling, saving, or otherwise changing document pixels.
The View menu exposes clockwise, counterclockwise, and reset actions. Their
shortcuts are Ctrl+Alt+Right, Ctrl+Alt+Left, and Ctrl+Alt+0 respectively.

Rotation is per open document view and intentionally ephemeral. It does not add
an undo-history command, mark the document modified, alter the document width or
height, change sparse pixel/selection storage, or enter the native file format.
Closing and reopening a document starts at zero degrees. Destructive image
rotation remains a separate Planned workflow and requires its own resampling,
storage, persistence, and golden-output decisions.

The view state is one of four bounded values: 0, 90, 180, or 270 degrees
clockwise. Any number of clockwise/counterclockwise operations is normalized
into those four states. Scroll extents swap for odd quarter turns; painting,
selection overlays, visible-document bounds, and pointer-to-document hit testing
share the same invertible transform. Rendering remains viewport-bounded for the
300,000-pixel canvas limit and does not allocate a rotated document image.

The canvas accessible description reports the current clockwise degree value.
Actions are disabled with no open document, and reset is disabled at zero
degrees. Offscreen keyboard evidence verifies action state, accessible rotation
text, reset, and unchanged document dimensions, sparse blocks, composite,
history, and modified state.

`tests/fixtures/view-rotation-quarter-turn.json` is an independent asymmetric
six-pixel orientation fixture. It verifies clockwise output and rotated pointer
selection mapping; its provenance is recorded alongside the fixture. Automated
resource evidence also rotates a sparse 300,000×200,000 document 1,025 times and
renders it at 1% zoom without materializing the full canvas.

This is a strict Partial capability. Mouse/trackpad rotation gestures, arbitrary
angles, persistent per-document view state, live Wayland latency, and live
assistive-technology announcements are not claimed.
