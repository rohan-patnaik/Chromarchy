# Viewport-bounded pixel grid

The View → Show Pixel Grid action (`Ctrl+Alt+G`) overlays image-pixel
boundaries on the current document tab. The setting is ephemeral and per tab.
It does not alter image pixels, selections, document history, dirty state,
storage blocks, or native persistence.

## Visibility and rendering bound

The grid becomes visible at 800% zoom and remains enabled but hidden below that
threshold. It follows the current 0°, 90°, 180°, or 270° non-destructive view
rotation. Lines use a fixed black source-over overlay at opacity 112; no color
or display-pipeline interpretation is introduced.

Painting iterates only pixel boundaries in `visibleDocumentRect()`. For a
visible rectangle of width `w` and height `h`, at most `w + h + 2` lines are
submitted. At maximum 3200% zoom in the 640 × 480 resource fixture, that is at
most 40 lines even for a sparse 300,000 × 300,000 document. No full-canvas grid
allocation, scan, cache, or retained geometry exists.

## Evidence and limits

An original four-by-three opaque-white fixture independently fixes the zoom
threshold, line opacity, and expected blended boundary color. Tests cover
visibility state, accessible descriptions, keyboard toggling, per-tab state,
unchanged document bytes/history, 1,025 hostile toggles, rotation compatibility,
and the 300k sparse resource bound. A dedicated benchmark measures the same
maximum-zoom sparse path.

This is a Partial grid/snapping capability. Grid spacing/color configuration,
non-pixel grids, guides, snapping targets and geometry, persisted view state,
pointer menu evidence, and live Wayland/assistive-technology behavior are not
implemented or claimed.
