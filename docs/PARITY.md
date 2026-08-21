# Workflow evidence policy

Chromarchy measures local professional editing workflows, not product-name
parity. The canonical, stable-ID source is
[`offline-capabilities.json`](offline-capabilities.json); its generated review
view is [`OFFLINE_PARITY.md`](OFFLINE_PARITY.md). Never edit the generated file.

The catalog currently covers all planned local surfaces:

- foundation, Omarchy integration, packaging, settings, workspaces, shortcuts,
  offline operation, accessibility, HiDPI, themes, and localization;
- typed pixels, alpha/channels, tiles, caches, scratch, jobs, render DAG,
  deterministic CPU rendering, history, persistence, recovery, failure and
  cancellation behavior;
- document lifecycle, tabs, recents, image/canvas sizing, image/view rotation,
  rulers, guides, grids, snapping, zoom, pan, navigator, menus, and panels;
- flat and nested layers, groups, multi-layer operations, masks, channels,
  blending, adjustments, effects, filters, and linked/embedded local sources;
- paint/input/presets, selections, local non-generative subject/background
  removal, transforms, crop, clone/heal/patch, and tonal/blur tools;
- paths, shapes, SVG interchange, OpenType, RTL, vertical/variable/path text;
- native, PNG, JPEG, TIFF, WebP, OpenEXR, HEIF/AVIF, PSD/PSB, RAW, metadata,
  professional color, print, and PDF proof workflows;
- actions, batch/CLI, scripting, plugins, frame animation, GIF/APNG, and video;
- corrupt-input, low-disk, release packaging, SBOM, license, and security work;
- explicit exclusions for generative/cloud/vendor-proprietary workflows and
  unsubstantiated product-level compatibility claims.

## Evidence and status rules

Every catalog row has a permanent ID, acceptance recipe, test/fixture owner,
dependency list, measurable budget, status, evidence list, and explicit limits.
IDs are never reused. A row is **Complete** only when all evidence applicable to
that row exists: automated coverage, visual goldens for pixel output,
undo/redo for mutations, persistence for document state, and budget/large-data
coverage. A usable but incomplete slice is **Partial**. **Planned** records
scope without implying implementation. **Blocked** names a real unresolved
decision or prerequisite. **Excluded** records an intentional product boundary.

CI validates the JSON schema and evidence paths and regenerates the Markdown in
memory. Any status/evidence change without the matching generated document makes
CI fail. Run:

```sh
python scripts/generate_offline_parity.py
python scripts/generate_offline_parity.py --check
```

Unknown fields, invalid phase/status combinations, non-normalized evidence
paths, and unstructured test anchors are rejected. Test evidence uses the
repository-relative `tests/path.cpp#testMethod` form and must resolve to an
actual method definition. Every Complete row needs at least one such test;
documentation or source paths alone are insufficient. Every active or retired
ID remains in the append-only `issued` registry in
`capability-id-history.json`. Removing an active ID requires an immutable
retirement entry; CI compares the registry with the GitHub base revision and
rejects removal or mutation of issued/retired history.

## Dependency decisions

lcms2, OpenColorIO, LibRaw, Exiv2, direct OpenEXR, libheif, FFmpeg, ONNX,
Poppler, scripting runtimes, and similarly consequential dependencies or models
require a clean decision packet and explicit user approval before adoption.
Catalog rows remain Planned while dependency-free prerequisite work proceeds.

## Claim boundary

Documentation may cite individual Complete stable IDs. It must not claim broad
Photoshop parity, proprietary behavior, or compatibility not demonstrated by
public fixtures and the catalog. Adobe code, APIs, UI, icons, strings, layouts,
models, and sample assets are not inputs to Chromarchy.
