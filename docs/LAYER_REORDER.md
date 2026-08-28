# Flat layer reorder

Layer → Move Layer Up (`Ctrl+Shift+]`) and Move Layer Down (`Ctrl+Shift+[`)
move the active flat pixel layer by one position. Both commands preserve the
active layer selection, expose correct enabled state at the top and bottom
boundaries, and participate in the existing document undo/redo and dirty-state
history.

Reordering is metadata-only. It changes compositing order but does not mutate,
copy, decode, or allocate pixel tiles. Native save/reopen preserves the order
and active layer. The layer list retains its accessible row interfaces while
their names and selected state refresh to represent the new order.

## Boundary and resource evidence

An original three-layer fixture fixes the inverse portable shortcuts. The UI
workflow exercises both directions, boundary disablement, deterministic
composite output, undo/redo clean-state identity, unchanged storage blocks, and
native persistence. A 1,025-cycle down/up test and benchmark use three one-tile
layers on a sparse 300,000 × 300,000 document and require byte-identical storage
block metadata afterward.

Nested groups, multi-layer selection, pointer drag reorder, user-configurable
shortcuts, and live assistive-technology behavior remain incomplete. The
related catalog rows therefore remain Partial.
