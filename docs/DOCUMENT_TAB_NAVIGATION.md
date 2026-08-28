# Keyboard document-tab navigation

Chromarchy exposes **Window → Next Document** (`Ctrl+PageDown`) and **Window →
Previous Document** (`Ctrl+PageUp`) for local, pointer-free movement among open
documents. Navigation follows the current visual tab order, including after a
tab is reordered, and wraps at both ends. The destination canvas receives
keyboard focus so canvas commands can continue without an extra focus step.

Both actions are disabled with zero or one open document. Switching tabs does
not read or write document files, allocate pixel tiles, add history entries, or
change dirty state. The permanent contract reorders three maximum-dimension
empty sparse documents, verifies both wrap directions and destination focus,
then performs 1,025 switches within the existing 50-millisecond synchronous
dispatch budget per switch. Because the documents remain sparse, the workflow
does not scan or allocate their 300,000 × 300,000 canvases.

This is a strict Partial Window-menu capability. Panel visibility commands,
panel-focus shortcuts, tab-list search, move-to-window behavior, workspace
reset/persistence, pointer navigation, live Wayland latency, and live
assistive-technology announcements remain unimplemented or unverified.
