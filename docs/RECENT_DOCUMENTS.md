# Recent documents privacy and bounds

Chromarchy keeps a local most-recently-used file list to support the File →
Open Recent workflow. The list uses the same Qt user-settings store already used
for workspace geometry and contains normalized absolute paths only. It never
stores document bytes, thumbnails, extracted metadata, or content previews and
does not perform network synchronization or telemetry.

The list is ordered most-recent first, deduplicated, and hard-limited to 20
entries. Each stored path is limited to 4096 UTF-8 bytes. Empty, relative,
over-limit, duplicate, and missing-file entries are discarded when settings are
read. At most the first 20 stored candidates are inspected, and overlong text is
rejected before path normalization or UTF-8 conversion. A file that disappears
after its menu action was built is removed again at activation time without
attempting a decode.

The first nine entries expose Ctrl+Alt+1 through Ctrl+Alt+9 in addition to menu
mnemonics. Ctrl+Alt+Shift+Delete clears the entire private path list without
closing any open document. Successfully opened raster/native files and
successfully saved native destinations become recent; failed opens and saves do
not.

The limits and hostile classes are anchored by the original fixture
`tests/fixtures/recent-documents-hostile.json`. Tests redirect Qt settings to a
temporary directory and verify ordering, deduplication, bounds, pruning,
persistence, path-only privacy, trigger-time disappearance, keyboard open, and
keyboard clear behavior.
