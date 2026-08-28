# Fit Canvas to View

View → Fit Canvas to View (`Ctrl+Shift+0`) performs a one-shot zoom change for
the current document tab. It chooses the smaller horizontal or vertical scale
for the canvas's current 0°, 90°, 180°, or 270° view orientation, then applies
the existing supported zoom clamp of 1% through 3200%.

This is display state only. The action does not change image dimensions,
pixels, selection, storage blocks, history, dirty state, or native persistence.
It is not sticky: later viewport resize or view rotation does not automatically
refit the canvas.

## Boundary and resource behavior

The operation uses constant-size viewport and rotated-document geometry and
does not inspect, allocate, or scan pixel tiles. An asymmetric 400 × 200
fixture independently exercises a width-limited unrotated fit and a
height-limited quarter-turned fit. Keyboard tests exercise the same two paths
through the View action and verify unchanged document state.

For a 300,000 × 300,000 sparse canvas, the scale needed by a normal viewport is
below the existing 1% minimum. The action therefore clamps at 1%, intentionally
leaving only a viewport-bounded portion visible. A 1,025-repeat resource test
and benchmark cover that outcome without changing sparse storage. This slice
does not lower the minimum zoom or claim that maximum-size canvases always fit
fully; such a change requires separate rendering/resource evidence.

## Remaining limits

Fit-to-selection, persistent/sticky fitting, resize-triggered refit, animated
transitions, pointer-menu evidence, live Wayland latency, and live assistive-
technology announcements remain incomplete. The related zoom and View-menu
capabilities therefore remain Partial.
