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

The live pixel quota and mutation-result boundary remain unapproved. The facts,
options, recommendation, and required owner choices are recorded in
[`docs/LIVE_TILE_STORAGE_DECISION.md`](docs/LIVE_TILE_STORAGE_DECISION.md);
dependency-free work must not infer a quota or compatibility policy from it.

The first item is in progress: dependency-free sample/channel/alpha/endian
descriptors, checked row/tile/allocation arithmetic, and bounded RGBA8
straight/premultiplied adapters exist. Bounded packed high-depth tiles now
provide raw sample access, copy-on-write mutation, and explicit conversion and
persistence seams. A bounded sparse typed owner provides lazy allocation,
byte-zero elision, exact decoded-payload accounting, and checked coordinates,
plus deterministic owning snapshot records with atomic bounded rebuild, but
remains isolated from documents and defines no file format. Checked unsigned
8/16-bit Gray, GrayAlpha, RGB, and straight RGBA conversion now reaches the
existing RGBA8 premultiplied boundary with explicit endian and rounding policy.
Sparse typed regions can be materialized deterministically at that boundary
under one combined source-plus-destination working-set cap.
Checked rectangular raw-byte access crosses sparse tile boundaries with atomic
writes, alias-safe staging, and zero-tile elision. Deterministic bounded before/after tile records
apply conflict-atomically in either direction as an isolated reversible-delta
prerequisite; they do not yet replace document snapshot history or implement
coalescing. Floating-point conversion, document integration, rendering, and
native typed persistence remain future slices.
The live RGBA8 document path now canonicalizes byte-zero pixel tiles produced
by merge, flatten, and native v1/v2 load without changing the native wire
format; declared legacy records continue to consume the existing hostile-input
budgets before decode.
Bounded snapshot history assigns a stable in-process identity to each retained
document state. The document view associates that identity with an initially
clean or explicitly established clean state; a native save establishes it only
after success. Undo/redo therefore returns to clean exactly at that state,
failed saves remain dirty, and a new branch cannot inherit a discarded save
point. These identities are dirty-state bookkeeping, not persisted document
revisions.
Sparse selection masks now canonicalize tiles that equal their base coverage
after live point/rectangle edits and native v2 load. Declared legacy records
remain charged against existing hostile-input budgets before elision, and the
wire format is unchanged.
Select All, Invert, and Deselect now expose document-level accessible selection
state and correct no-document action enablement. Keyboard tests cover sparse
300k-canvas transitions, undo/redo clean-state identity, and native save/reopen
without changing selection storage or persistence limits. Full and empty state
classification is semantic even when an equivalent rectangle remains encoded
as sparse exception tiles, so no-op Select All/Deselect does not dirty history.
The CPU source-over path is anchored by an original, independently calculated
full-image golden across the x=255/256 tile boundary. Repeated full and bounded
region rendering, the painter entry point, native save/reopen, flatten, and
merge-down persistence are checked without changing the RGBA8 renderer or its
color interpretation.
An independently framed fixed PNG now anchors byte-preserving
import/edit/export behavior. Export writer failure after atomic temporary-file
open is exercised against an existing destination, which remains unchanged and
leaves no temporary file; commit/power-loss injection remains incomplete.
The current document/layers workspace now exposes stable Qt accessibility
names and descriptions, an explicit core layer-control tab order, and tested
keyboard dispatch for selection, layer creation/duplication, undo, and redo.
The bounded New Document dialog additionally has named controls and tested
keyboard create/cancel paths. This is an offscreen-tested subset; remaining
dialog/action traversal and live AT-SPI/screen-reader verification remain open.
The single-document Unsaved Changes prompt now exposes named Save, Discard, and
Cancel actions with deterministic focus/default/escape behavior; keyboard
tests cover all three choices and reopen the native Save result.
Layer rename now has an explicit F2 action and accessible bounded dialog;
offscreen tests cover cancel, over-budget rejection, Unicode rename,
undo/redo dirty-state identity, and native save/reopen equivalence.
Flat layer visibility now exposes stable accessible row names/descriptions and
checked state across metadata refreshes. A keyboard-only Space toggle is
covered through composite output, undo/redo clean-state identity, and native
save/reopen persistence.
Flat layer opacity now has keyboard-entry evidence for a bounded fractional
percentage, including the standard accessible value interface, deterministic
composite output, undo/redo clean-state identity, and native save/reopen
persistence.
Flat pixel-layer lock now has keyboard Space and accessible checked-state
evidence through rejected pixel mutation, undo/redo clean-state identity, and
native save/reopen enforcement.
Flat layer reorder now has keyboard shortcut and accessible selection-continuity
evidence through deterministic composite order, unchanged shared pixel blocks,
undo/redo clean-state identity, and native save/reopen persistence.
Sparse pixel-layer creation now has keyboard and accessible active-row evidence
through zero-tile allocation, unchanged existing payload blocks/composite,
undo/redo clean-state identity, and native save/reopen persistence. Layer-count
refreshes preserve surviving row interfaces instead of clearing the list.
Pixel-layer duplication now has keyboard and accessible active-row evidence
through shared COW block identity, unchanged composite output, undo/redo
clean-state identity, and native save/reopen persistence of both layers.
Flat merge-down now has Ctrl+E and accessible surviving-row evidence through
correct lock/single-layer action enablement, deterministic composite output,
sparse storage, undo/redo clean-state identity, and native save/reopen.
Flat document flattening now has keyboard menu-mnemonic and accessible
surviving-row evidence through lock-aware action enablement, deterministic
composite output, sparse storage, undo/redo identity, and native save/reopen.
Flat pixel-layer removal now has Delete-key and accessible row-remapping
evidence through sparse payload release/restoration, single-layer no-op action
state, undo/redo clean identity, and native save/reopen.
The local recent-document path list now uses the established Qt settings store
with most-recent ordering, deduplication, missing-file pruning, privacy clear,
and hard 20-entry/4096-byte path bounds. Open Recent supplies keyboard shortcuts
for the first nine entries and a keyboard clear action; hostile settings and a
file disappearing after menu construction are handled without decoding or
retaining document content.
Non-destructive view rotation now supplies clockwise, counterclockwise, and
reset keyboard actions over a four-state quarter-turn transform. Independent
orientation, rotated pointer mapping, accessible state, unchanged document
bytes/history, hostile repetition, and viewport-bounded 300k-class sparse
rendering are covered without entering image-rotation or persistence decisions.
Bundled offline Help and About now expose a six-topic, fixed-text local
reference through F1 and the Help menu. Offscreen tests cover keyboard topic
navigation, stable accessible metadata, exact project-license text, required
format/resource/diagnostic content, a 500-ms opening budget, and hard per-page
and combined text caps without shell, filesystem-enumeration, or network work.
Full third-party license inventory, localization, live AT-SPI behavior, and
complete dialog traversal remain later release-quality work.
The canvas now offers an ephemeral pixel grid through Ctrl+Alt+G. It becomes
visible at 800% zoom, follows all four view-rotation states, and draws only the
pixel boundaries intersecting the visible document rectangle. Independent
overlay color, keyboard/per-tab state, accessible description, 1,025-toggle,
300k sparse-canvas, and benchmark evidence leaves document pixels, history,
selection, native persistence, and storage untouched. Configurable grids,
guides, snapping, persisted view state, and live Wayland latency remain open.

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
