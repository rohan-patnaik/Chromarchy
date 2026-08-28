# Recent-document fixture provenance

`recent-documents-hostile.json` is an original Chromarchy policy fixture. It
defines the independently testable entry and UTF-8 path limits plus hostile
settings classes; it contains no third-party paths, content, UI text, or sample
assets.

The settings tests create their own temporary local files and substitute those
paths into the fixture classes. File payloads contain a sentinel that must never
appear in persisted settings, proving that the recent list records paths only.
