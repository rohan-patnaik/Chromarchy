# Chromarchy product and implementation plan

## Product boundary

The goal is a fast, local professional image editor with broad non-generative Photoshop workflow coverage. Literal full parity with current Photoshop is not a credible near-term claim: Photoshop represents decades of proprietary behavior, format edge cases, print/video/3D integrations, extensions, and platform services. Progress is measured by a public capability matrix and reproducible workflows, not by marketing language.

Excluded by request: generative fill, generative expand, text-to-image, partner AI models, and cloud-only Adobe services. Also excluded unless independently re-scoped: Adobe account, Stock, Express, Firefly Boards, and Creative Cloud collaboration.

## Architecture

- C++23 with Qt 6 Widgets for mature docking, input, accessibility, printing, Wayland, and low runtime overhead.
- RAII, QObject parent ownership, smart pointers, spans, and value types; no raw owning pointers.
- Tile-based document engine with copy-on-write pixel tiles, dirty-region tracking, mip previews, background jobs, and bounded caches.
- GPU compositing through Qt RHI, with deterministic CPU fallbacks for export and tests.
- LittleCMS/OpenColorIO-based color pipeline; 8/16-bit integer and 16/32-bit float channels.
- Versioned native document container; PSD/PSB import/export treated as compatibility adapters with fixture tests.
- Command journal for undo/redo and crash recovery; autosave never overwrites the source document.
- Native plugin API is deferred until the core data model is stable.

## Milestones

### M0 — foundation

- Native window, dockable workspace, keyboard routing, theme bridge, logging, settings, CI, packaging skeleton.
- Omarchy manifest/launcher, install diagnostics, safe failure when the app is missing.
- Test harness and benchmark fixtures.

### M1 — document core

- New/open/save/export, zoom/pan/rotate canvas, multiple tabs, rulers/guides/grid.
- Pixel layers, groups, visibility, opacity, locks, reorder, duplicate, merge, flatten.
- Non-destructive selection model and command-based undo/redo.
- PNG/JPEG/TIFF/WebP/OpenEXR import/export with metadata policy.

### M2 — paint, selection, transform

- Brush/pencil/eraser engine with pressure, tilt, spacing, smoothing, symmetry, presets.
- Rect/ellipse/lasso/polygon/magic-wand/quick selection; feather, grow, shrink, invert.
- Move, crop, free transform, perspective, warp, content-aware scale only if independently implemented.
- Gradient, fill, color picker, clone stamp, healing, patch, dodge, burn, sponge, blur, sharpen, smudge.

### M3 — compositing and nondestructive editing

- Complete documented blend-mode set, clipping masks, layer/vector masks, channels.
- Adjustment layers: levels, curves, exposure, vibrance, hue/saturation, color balance, B&W, LUT.
- Layer styles and smart-filter graph.
- Smart-object equivalent with linked/embedded assets using original terminology.

### M4 — vector, text, automation

- Paths, pen, shapes, boolean operations, stroke/fill, SVG interchange.
- Horizontal/vertical text, OpenType shaping, RTL, text-on-path, variable fonts.
- Actions/macros, batch processing, droplets/CLI, configurable shortcuts, workspace presets.

### M5 — professional color and format parity

- Soft proof, gamut warnings, profile conversion/assignment, CMYK/Lab/spot workflows.
- RAW development adapter, Camera-Raw-style nondestructive settings with original UI.
- High-quality PSD/PSB round trips, large documents, print setup, PDF proof export.
- Performance budgets for startup, brush latency, zoom, memory, save, and export.

### M6 — release quality

- Accessibility, localization, tablet matrix, crash recovery, corrupt-input fuzzing.
- Reproducible Arch package/AppImage, signed checksums, SBOM, security policy.
- Omarchy validation on a real Quattro machine and marketplace screenshots/submission.

## Definition of done per feature

Each capability requires unit/integration tests, at least one golden fixture where visual output matters, undo/redo coverage, large-document behavior, keyboard/accessibility handling, documentation, and a focused commit pushed after CI passes.

