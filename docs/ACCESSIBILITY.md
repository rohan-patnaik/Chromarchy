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
Metadata-only refreshes retain row identity when the layer count is unchanged,
so assistive interfaces do not become stale across visibility, rename, opacity,
lock, undo, or redo updates.

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
The flat reorder path invokes the documented keyboard shortcut from the layer
list and verifies that the active selection follows the moved layer. Held row
interfaces remain valid and expose the new row names through reorder and undo,
with composite order, shared pixel storage, and native persistence checked.

This is a partial accessibility slice, not a completed audit. File chooser,
save-as/export, and error dialogs; multi-document close/quit prompts; every menu
and action state; nested or multi-selected layers; complete focus-trap
traversal; high-contrast and scale behavior; live AT-SPI event/state exposure;
and a real screen-reader workflow remain unverified. Packaged launch evidence
may confirm that the application starts without taking the active workspace,
but it does not replace live assistive-technology validation.
