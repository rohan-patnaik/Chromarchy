# Bundled Offline Help and About

Chromarchy's Help menu exposes two local actions:

- **Offline Help** (`F1`) opens the Overview topic.
- **About Chromarchy** opens the same dialog on its About topic.

The dialog contains six read-only topics: Overview, Shortcuts, Formats and
Limits, Diagnostics, Licenses, and About. `Ctrl+Tab` moves between topics and
`Escape` closes the dialog. It is available with no document open.

## Offline and resource boundary

All topic text is compiled into the application. Opening or navigating Help
does not read a remote resource, enumerate personal files, execute a shell
command, send telemetry, or require an account. Diagnostics are limited to
bounded application/Qt identifiers, process word size, and already-compiled
resource constants.

Each topic is capped at 8192 characters and the six topics at 49152 characters
combined. The offscreen integration test enforces both caps, rejects HTTP(S)
content, checks required contract phrases, and requires the full keyboard test
path—including dialog verification and close—to finish within 500 ms.

## Truth and licensing limits

Formats and Limits reports the current native and Qt-codec surface plus the
same hard constants used by the implementation. Licenses embeds the exact
repository `LICENSE` text and states that Qt and optional system codecs retain
their own licenses.

This is a Partial capability. It is not a generated dependency inventory or
SBOM, does not bundle every third-party license text, and has no localization,
live AT-SPI/screen-reader, pointer-traversal, or complete focus-trap evidence.
