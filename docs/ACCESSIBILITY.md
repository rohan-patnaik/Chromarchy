# Accessibility evidence and limits

Chromarchy exposes original accessible names and descriptions for the current
document workspace, document tabs, canvas, layers panel, layer list, layer
actions, opacity control, and pixel-lock control. The flat layers panel has an
explicit tab sequence from the layer list through opacity and lock controls.
The New Document dialog exposes names and descriptions for its bounded width,
height, and action controls, with explicit dimension focus order.
The Unsaved Changes prompt has stable dialog and action metadata, explicit
Save-to-Discard-to-Cancel focus order, Save as the default action, and Cancel
as the Escape action.
The Rename Layer dialog exposes a named, described text editor and action
group, bounds editor input, and is reachable with F2 without moving focus to
the layers panel.
Flat layer rows expose their names, visibility descriptions, and checked state.
Metadata-only refreshes update surviving rows in place and grow or shrink only
at the trailing edge, so held interfaces do not become stale across visibility,
rename, opacity, lock, reorder, create, undo, or redo updates.

`MainWindowTest` queries Qt's accessibility interfaces offscreen and verifies
that the core workspace objects have non-empty roles, stable names, and
descriptions. It also drives a pointer-free path through those layer controls
and exercises Select All, create layer, duplicate layer, undo, and redo by
keyboard. The redo test guards against duplicate platform bindings that Qt
would otherwise reject as ambiguous. It also opens the New Document dialog by
shortcut, traverses and enters bounded dimensions, creates the document with
Enter, and verifies that Escape cancels without adding another document.
Close-prompt tests drive Escape/Cancel, tab-and-Space/Discard, and
Return/Save from the focused controls; the save path is reopened to verify that
the edit persisted before the tab closed.
The multi-tab quit path invokes Ctrl+Q from the canvas, makes each modified
document current before prompting, and verifies the prompt's document name and
accessible metadata. Cancel retains every tab with the canceled document
active; retry skips the already saved clean tab and resolves the remaining
documents in order through keyboard Discard and Save. Live announcement and
screen-reader behavior remain unverified.
The Window menu has a stable accessible name and description. Ctrl+PageDown
and Ctrl+PageUp navigate next and previous documents in current visual tab
order, wrap at both ends, and focus the destination canvas. Offscreen tests
cover zero/one-tab disabled state, reordered tabs, both wrap directions, and
retained focus over repeated maximum-dimension sparse-document navigation;
live tab-change announcements remain unverified.
The layer rename path verifies F2 dispatch, Escape cancellation, rejection of
an over-budget UTF-8 name without dirtying the document, Unicode rename,
undo/redo dirty-state identity, and save/reopen persistence.
The visibility path focuses the layer list, toggles the selected row with
Space, verifies the accessible checked state and deterministic composite before
and after undo/redo, then saves and reopens the hidden-layer result.
The opacity path reaches the bounded percentage spin box by keyboard, enters a
fractional value, and verifies its standard accessible value range and current
value. It also covers deterministic composite output, undo/redo saved-state
identity, retained accessibility state, and native save/reopen equivalence.
The pixel-lock path reaches the named checkbox by keyboard, toggles it with
Space, and verifies its retained accessible checked state. Locked pixel writes
are rejected before and after native reopen, while undo/redo and save preserve
the expected clean/dirty identity.
The flat reorder path invokes both documented inverse keyboard shortcuts from
the layer list and verifies that the active selection follows the moved layer.
The actions expose correct top/bottom boundary enablement. Held row interfaces
remain valid and expose the new row names through reorder and undo, with
composite order, shared pixel storage, and native persistence checked.
The create path invokes the documented keyboard shortcut, keeps the surviving
row interface valid across create/undo/redo count changes, and verifies the new
active row metadata. The empty layer allocates no tiles, preserves existing
pixel payload sharing and composite output, and survives native save/reopen.
The duplicate path invokes the documented keyboard shortcut and verifies the
generated copy name, active accessible row, and retained surviving interface
through duplicate/undo/redo. Both live layers reference the same COW pixel
block until mutation, and native save/reopen preserves both pixel payloads.
The merge-down path invokes Ctrl+E and verifies that the surviving active row
keeps its accessible interface and name through merge/undo/redo count changes.
The action exposes correct locked/single-layer enablement, while deterministic
composite output, sparse storage, dirty identity, and native reopen are checked.
The flatten path invokes the Layer menu mnemonic entirely by keyboard and keeps
the surviving row interface valid as it becomes the active Flattened row.
Lock-aware action state, composite output, sparse storage, undo/redo identity,
and native reopen are checked across the three-to-one layer transition.
The remove path invokes Delete from the canvas and verifies active-row remapping
when a middle layer is removed. Surviving row interfaces expose their rewritten
names through remove/undo/redo, while released storage, single-layer action
disablement, dirty identity, and native reopen are checked.
The document view describes whether no pixels, the entire canvas, or a partial
pixel selection is active. Select All, Invert, and Deselect expose stable
shortcuts and are disabled without a document. A keyboard-only 300k-canvas
workflow verifies sparse selection state, description updates, undo/redo clean
identity, and native save/reopen for partial-inverted and full selections.
Equivalent full-rectangle and empty-inverse tile representations are classified
by semantic canvas coverage rather than by base coverage or tile count alone.

The File → Open Recent menu has a stable accessible name and description. Its
first nine bounded local entries expose Ctrl+Alt+1 through Ctrl+Alt+9, and
Ctrl+Alt+Shift+Delete clears stored paths without closing open documents. An
offscreen keyboard test opens a native recent document, removes an entry that
disappeared after menu construction without decoding it, and clears the list.

The canvas accessible description reports its non-destructive clockwise view
rotation as 0, 90, 180, or 270 degrees. Ctrl+Alt+Right and Ctrl+Alt+Left rotate
the current view by a quarter turn, while Ctrl+Alt+0 resets it. An offscreen
keyboard test verifies enabled/reset action state, announced degree text, and
that rotation does not modify document pixels, dimensions, history, or dirty
state. Live assistive-technology event announcements remain unverified.

The Help menu and bundled Offline Help dialog expose stable accessible names,
descriptions, and roles. F1 opens the Overview without a document; Ctrl+Tab
navigates six read-only named topics; Escape closes the dialog. The About menu
action opens the same bounded dialog directly on its named About topic. The
offscreen test verifies all topic controls and content bounds, but does not
replace live AT-SPI announcement or screen-reader traversal evidence.

The View menu has a stable accessible name and description. Ctrl+Alt+G toggles
the current tab's pixel grid without requiring pointer focus, and the canvas
description distinguishes disabled, enabled-below-threshold, and enabled-and-
visible state. Offscreen keyboard tests cover action checked state and per-tab
isolation; live assistive-technology state-change announcements remain open.

Ctrl+Shift+0 invokes Fit Canvas to View without pointer interaction. The canvas
accessible description reports the resulting rounded zoom percentage together
with rotation and pixel-grid state. Offscreen tests cover enabled/no-document
action state, unrotated and quarter-turned fitting, and unchanged document
history; live zoom-change announcements remain unverified.

When the canvas has focus, Arrow keys pan by 32 viewport pixels and Shift+Arrow
keys pan by 128 pixels without moving focus. The canvas description reports the
current horizontal and vertical scrollbar positions and maxima after keyboard,
programmatic, resize, zoom, or rotation changes. Offscreen tests cover both
axes, clamped boundaries, quarter-turn behavior, per-tab isolation, unchanged
document/history state, and a 300k sparse canvas; live pan announcements remain
unverified.

This is a partial accessibility slice, not a completed audit. File chooser,
save-as/export, and error dialogs; multi-document close/quit prompts; every menu
and action state; nested or multi-selected layers; complete focus-trap
traversal; high-contrast and scale behavior; live AT-SPI event/state exposure;
and a real screen-reader workflow remain unverified. Packaged launch evidence
may confirm that the application starts without taking the active workspace,
but it does not replace live assistive-technology validation.
