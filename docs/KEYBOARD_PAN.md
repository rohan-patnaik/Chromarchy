# Keyboard Canvas Pan

When the canvas has keyboard focus, the arrow keys pan the current viewport by
32 viewport pixels. Holding Shift pans by 128 pixels. Horizontal and vertical
movement is clamped to the existing scroll range, so repeated input cannot move
past the document bounds.

Keyboard pan is display-only and local to each open tab. It works in all four
quarter-turn view orientations, retains canvas focus, and updates the canvas
accessible description with the current horizontal and vertical positions and
their maxima. It does not alter document pixels, selection, layer storage,
history, dirty state, or native persistence.

The checked-in contract covers both axes, normal and accelerated steps,
boundary clamping, rotation, per-tab isolation, focus, and accessible state. A
1,025-pair hostile repeat and benchmark use a one-tile 300,000×300,000 sparse
document at 1% zoom and verify unchanged storage. Each key event performs only
a bounded scrollbar update; it does not scan or allocate the canvas.

This slice does not add mouse or trackpad gestures, momentum, configurable
steps, persisted view state, fit-to-selection, sticky resize fitting, or a
navigator. Live Wayland latency and assistive-technology announcements remain
unverified.
