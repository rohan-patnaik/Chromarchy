# Layers-panel visibility and focus

The Window menu exposes five pointer-free workspace commands:

- **Show Layers** (`Ctrl+Alt+Shift+L`) toggles the Layers panel and mirrors its
  current visibility with a checked action.
- **Focus Layers** (`Ctrl+Alt+L`) shows and raises the panel when necessary,
  then moves keyboard focus to the document layer list.
- **Focus Canvas** (`Ctrl+Alt+C`) returns keyboard focus to the current canvas
  and is disabled when no document is open.
- **Next Workspace Area** (`F6`) and **Previous Workspace Area** (`Shift+F6`)
  cycle between the current canvas and the Layers panel. With two areas the
  forward and reverse commands intentionally reach the same other destination.
  Either command reveals the Layers panel before focusing it; with no open
  document, Layers is the only destination.

These commands change only widget visibility and focus. They do not read or
write a document, allocate pixel tiles, create history, or change dirty state.
The repository's pre-existing opaque `QMainWindow::saveState()` path may retain
dock visibility when the application closes; this slice adds no settings key,
schema, migration, reset behavior, or workspace-persistence claim. The
permanent contract runs 1,025 visibility changes over an empty 300,000 ×
300,000 sparse document within the existing 100 ms synchronous per-toggle
budget, then verifies both direct focus destinations, 1,025 focus cycles from
multiple panel descendants, hidden-panel recovery, zero-document behavior, and
exact source-file preservation.

This remains a strict Partial Window/Layers workflow. Workspace persistence and
reset, additional focus-cycle destinations, detached/floating panel behavior,
pointer coverage, live Wayland latency, and live assistive-technology
announcements remain unimplemented or unverified.
