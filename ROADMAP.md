# Chromarchy offline implementation roadmap

## Product boundary

Chromarchy is a fast, local-first professional image editor for current Arch
Linux, Wayland, and Omarchy Quattro. Progress is reported by stable capability
IDs in `docs/offline-capabilities.json`, never by broad Photoshop-parity claims.
Generative image editing, mandatory cloud services, vendor accounts/private
APIs, and copied proprietary expressive material are excluded.

## Architectural invariants

- C++23 and Qt 6; RAII, value types, explicit ownership, no raw owning pointers.
- Preserve and evolve the sparse tiled copy-on-write engine; do not replace it
  with a disposable full-frame architecture.
- All caches, scratch, history, queues, and background work are bounded and
  cancelable. CPU reference rendering remains deterministic.
- Offline operation is the default; no telemetry or mandatory network service.
- Every pixel mutation needs goldens, undo/redo, persistence, corrupt-input and
  budget evidence where applicable.
- Consequential dependencies or models require a decision packet and explicit
  user approval before adoption. Dependency-free prerequisite work continues.

## Phase gates

### Phase 0 — evidence baseline

- Maintain the complete stable-ID machine catalog and generated
  `docs/OFFLINE_PARITY.md` covering formats, menus, tools, panels, local
  non-generative selection/removal, media, automation/plugins, print/PDF,
  accessibility/localization, professional color, release work, and exclusions.
- Record acceptance, owner, dependencies, budgets, status, evidence, and limits
  on every row. CI rejects schema, evidence-path, and generated-output drift.
- Stop for independent high review before changing engine contracts.

### Phase 1 — internal engine contracts

1. Typed/checked pixel storage: explicit depth, channel layout, color meaning,
   premultiplication and conversion boundaries.
2. Shared bounded tile/cache/scratch/job services with cancellation, priorities,
   low-memory/low-disk behavior, and deterministic test scheduling.
3. Partial render DAG with dirty-tile propagation and deterministic CPU
   reference nodes; GPU acceleration stays an optional equivalent path.
4. Reversible delta commands and stroke coalescing with strict byte/count limits
   after every cursor transition.
5. Chunked/versioned native persistence, migration fixtures, bounded reads,
   streaming writes, corrupt/truncated coverage, crash journal, and recovery.

Stop for independent high review and remediate findings before M1 closure.

The first item is in progress: dependency-free sample/channel/alpha/endian
descriptors, checked row/tile/allocation arithmetic, and bounded RGBA8
straight/premultiplied adapters exist. Bounded packed high-depth tiles now
provide raw sample access, copy-on-write mutation, and explicit conversion and
persistence seams. A bounded sparse typed owner provides lazy allocation,
byte-zero elision, exact decoded-payload accounting, and checked coordinates,
plus deterministic owning snapshot records with atomic bounded rebuild, but
remains isolated from documents and defines no file format. High-depth numeric
conversion, document integration, rendering, and native persistence remain
future slices.

### Phase 2 — close M0/M1 local workflows

- Workspace/settings/workspace presets, shortcut editor, accessible names/focus,
  keyboard navigation, localization readiness, HiDPI and theme behavior.
- Document lifecycle, recents, recovery, single-instance activation, multi-tab
  prompt/revision semantics, cancellation and low-disk handling.
- Image/canvas size, image/view rotate/flip, rulers, guides, units, grid, pixel
  grid, snapping, navigator, zoom/pan/gesture behavior.
- Nested groups, multi-layer selection/operations, layer tree persistence,
  merge/flatten goldens, masks/channels prerequisites.
- Safe streaming import/export beyond the current 64-MiPixel full-frame guard
  where supported without a new dependency; metadata inspection/control and
  public golden corpora.
- Complete every applicable File/Edit/Image/Layer/Select/View/Window/Help menu
  and tool/panel acceptance row rather than treating presence as completion.

Stop for independent high review and remediate findings before tool work.

### Phase 3 — paint, input, presets, selections, transforms, retouch

- Brush/pencil/eraser, pressure/tilt/rotation, smoothing, spacing, symmetry,
  presets and tool-options panels with latency and stroke goldens.
- Rectangle/ellipse/lasso/polygon/color-range selections, combine modes,
  feather/grow/shrink/border/invert, masks, and traditional local selection.
- Local non-generative background removal and deterministic patch-based removal;
  no generative fill/expand.
- Move, crop, free/perspective/warp transforms, gradient/fill/picker,
  clone/heal/patch, dodge/burn/saturation, blur/sharpen/smudge.

### Phase 4 — compositing and nondestructive graph

- Public-formula blend modes, clipping/raster/vector masks, alpha channels.
- Adjustment nodes and properties, layer effects, deterministic filters,
  nondestructive filter graph, linked/embedded local sources.
- History/properties/channels/histogram/info panels with bounded refresh.

### Phase 5 — vector, type, automation

- Paths/pen, shapes, boolean geometry, strokes/fills, documented SVG subset.
- Editable OpenType text, fallback, RTL, vertical/variable/path text using
  original UI language and public fixtures.
- Stable command schema, actions/macros, batch/headless CLI. Scripting runtime
  and native plugin ABI proceed only after separate security/dependency review.

### Phase 6 — color, formats, print

- Typed 8/16-bit integer and 16/32-bit float paths before color adapters.
- ICC display/assign/convert, CMYK/Lab/spot, proof/gamut workflows only after an
  approved lcms2/OpenColorIO decision packet.
- PSD/PSB documented public-fixture adapter; RAW, HEIF/AVIF, direct OpenEXR and
  metadata dependencies only after their individual approvals.
- Local print setup and PDF proof export with explicit backend/color decisions.

### Phase 7 — media, plugins, and release quality

- Frame timeline and GIF/APNG. Video waits for an approved FFmpeg packet.
- Permission/version policy for scripts/plugins; no implicit network access.
- Accessibility and localization matrices, tablet hardware matrix, fuzz corpus,
  recovery/ENOSPC/cancel tests, reproducible packages/AppImage, signed checksums,
  SBOM, license inventory, security policy, and final real-Omarchy evidence.

## Definition of done

A capability changes to Complete only in the same focused commit that supplies
its acceptance evidence and regenerates the catalog view. Required evidence is
feature-specific but includes automated tests, original/public fixtures,
goldens for pixel/layout output, undo/redo and persistence for mutations,
bounded large-data behavior, keyboard/accessibility handling, documentation,
green exact-SHA Arch CI, and an independent gate review at phase boundaries.
