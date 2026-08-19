# Workflow capability matrix

Chromarchy tracks professional image-editing workflows by reproducible evidence, not by product-level parity claims. A capability is only marked **Complete** when its documented acceptance workflow, automated tests, and relevant visual fixture pass on the supported platform.

Status meanings:

- **Complete** — acceptance workflow and automated evidence are present.
- **Partial** — a usable vertical slice exists, but listed gaps remain.
- **Planned** — included in the roadmap but not implemented.
- **Excluded** — intentionally outside the product boundary.

## Evidence rules

Each completed row must link to:

1. the implementation milestone and user-facing workflow;
2. automated unit or integration coverage;
3. a golden fixture when pixels or layout are affected;
4. undo/redo coverage for mutating editor operations;
5. large-document or performance evidence where relevant.

Until all applicable evidence exists, the row stays Partial or Planned.

## Core workflows

| Workflow | Status | Milestone | Current evidence | Remaining evidence or gaps |
| --- | --- | --- | --- | --- |
| Native Wayland desktop workspace | Partial | M0 | Qt 6 Widgets application and dockable shell | Arch/Omarchy validation, settings persistence, accessibility test |
| Omarchy menu launch | Partial | M0 | Root manifest and Quickshell entry point | Missing-binary diagnostic and real Quattro validation |
| New raster document | Partial | M1 | Validated new-document dialog creates a real tiled document | Background options, presets, undo boundary |
| Open raster image | Partial | M1 | Qt image decoder adapter into sparse real pixel tiles, file-open UI and PNG integration coverage | JPEG/TIFF/WebP/OpenEXR fixtures, metadata policy validation |
| Native lossless save/load | Partial | M1 | Versioned atomic `.chromarchy` container, save/open UI, layered round-trip and corrupt/truncated-input tests | Groups/selections, recovery journal, forward-migration fixtures |
| Export raster image | Partial | M1 | Export UI, deterministic CPU composite, atomic image write, explicit JPEG alpha flattening, PNG round-trip test | JPEG/TIFF/WebP/OpenEXR fixtures, metadata controls |
| Multiple document tabs | Partial | M1 | Movable document tabs, dirty markers, per-document close and application-exit save prompts | Tab lifecycle integration tests, configurable keyboard navigation |
| Zoom, pan, rotate view | Partial | M1 | Visible-region compositing, bounded zoom, middle-button pan, canvas viewport tests | Rotation, pixel grid, gesture/tablet input, latency benchmark |
| Rulers, guides, and grid | Planned | M1 | — | Unit handling, snapping, persistence |
| Pixel layers and groups | Partial | M1 | Sparse copy-on-write tile storage, pixel-layer ordering, visibility, opacity, locks, and `chromarchy_document`/`chromarchy_tiled_image` tests | Groups, UI workflow, merge/flatten, golden fixtures, undo/redo |
| Duplicate, merge, and flatten layers | Partial | M1 | Undoable duplicate, tile-local merge-down and flatten UI operations with lock protection and composite-preservation tests | Group-aware behavior, golden fixtures, blend modes beyond source-over |
| Non-destructive selection model | Partial | M1 | Sparse copy-on-write grayscale coverage tiles, canvas rectangle workflow and coverage overlay, undoable select-all/deselect/invert, dirty regions, and 300k×300k sparse behavior test | Persistence, feathering and transform integration |
| Command undo/redo | Partial | M1 | Bounded snapshot commands with copy-on-write tiles, redo invalidation, layer-operation UI routing, and `chromarchy_history` tests | Paint-command coalescing, saved-state revision tracking, recovery journal |
| Brush, pencil, and eraser | Planned | M2 | — | Tablet input, dynamics, presets, latency fixtures |
| Geometric and freehand selections | Planned | M2 | — | Feather/grow/shrink/invert and golden masks |
| Move, crop, and transforms | Planned | M2 | — | Resampling policy, undo/redo, golden fixtures |
| Retouch and tonal tools | Planned | M2 | — | Clone/heal/dodge/burn/blur/sharpen/smudge workflows |
| Blend modes, masks, and channels | Planned | M3 | — | Documented blend set and conformance fixtures |
| Adjustment layers | Planned | M3 | — | CPU reference implementations, UI, golden fixtures |
| Layer effects and filter graph | Planned | M3 | — | Nondestructive graph, cache bounds, serialization |
| Linked and embedded source layers | Planned | M3 | — | Relinking, missing-source behavior, portability |
| Paths and vector shapes | Planned | M4 | — | Boolean geometry, SVG interchange, render fixtures |
| Professional text layout | Planned | M4 | — | OpenType, RTL, vertical text, text-on-path fixtures |
| Actions, batch, and CLI automation | Planned | M4 | — | Stable command schema, failure policy, integration tests |
| Color-managed proof workflows | Planned | M5 | — | Profile fixtures, CMYK/Lab/spot validation |
| RAW development adapter | Planned | M5 | — | Supported-camera policy and nondestructive settings |
| PSD/PSB compatibility adapter | Planned | M5 | — | Publicly sourced fixtures and documented round-trip limits |
| Print and PDF proof export | Planned | M5 | — | Color-managed layout and output fixtures |
| Recovery, fuzzing, and release packaging | Partial | M6 | CI-validated Arch `makepkg` development recipe that builds, tests and installs the native binary | Crash recovery, fuzz corpus, pinned release sources, AppImage, checksums and SBOM |

## Explicit exclusions

| Workflow or service | Status | Reason |
| --- | --- | --- |
| Generative fill or expand | Excluded | Product boundary; no generative image editing |
| Text-to-image or partner image models | Excluded | Product boundary; offline professional raster workflows first |
| Adobe Firefly, Stock, Express, or Boards | Excluded | Vendor cloud services and proprietary integrations |
| Adobe account and Creative Cloud collaboration | Excluded | Offline-first design; no mandatory account or cloud |
| Adobe proprietary extensions or private APIs | Excluded | Legal and architectural boundary |
| Adobe UI, icons, strings, layouts, or sample assets | Excluded | Chromarchy uses original terminology and visuals |

## Maintenance

Update this matrix in the same focused commit that changes a capability's status. Evidence links must point to files in this repository or stable CI artifacts. Marketing and release notes must not claim Photoshop parity; they may name specific completed workflows from this matrix.
