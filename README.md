# Chromarchy

Chromarchy is an offline-first professional raster image editor for Omarchy Quattro. It is an original product, not an Adobe project, and does not include Adobe code, assets, branding, or generative-fill services.

The repository contains two products that ship together:

- A native C++23/Qt 6 desktop application.
- A small Omarchy Quattro `menu` plugin that launches the native application.

## Status

Foundation and document-core build. The stable-ID offline evidence backlog is
generated at [docs/OFFLINE_PARITY.md](docs/OFFLINE_PARITY.md); status policy and
claim limits are in [docs/PARITY.md](docs/PARITY.md), and dependency-ordered
implementation gates are in [ROADMAP.md](ROADMAP.md).

## Build on Arch/Omarchy

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-imageformats
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/chromarchy
```

Core performance workloads and their exact commands are documented in [docs/PERFORMANCE.md](docs/PERFORMANCE.md).
Raster codec behavior and the default metadata policy are documented in [docs/FORMATS.md](docs/FORMATS.md).
Recent-file privacy and resource bounds are documented in
[docs/RECENT_DOCUMENTS.md](docs/RECENT_DOCUMENTS.md).
Non-destructive quarter-turn canvas behavior is documented in
[docs/VIEW_ROTATION.md](docs/VIEW_ROTATION.md).
Bundled Help/About content, its offline boundary, and hard text budgets are
documented in [docs/OFFLINE_HELP.md](docs/OFFLINE_HELP.md).
The non-destructive viewport-bounded pixel grid is documented in
[docs/PIXEL_GRID.md](docs/PIXEL_GRID.md).
Rotation-aware one-shot canvas fitting within the supported zoom range is
documented in [docs/FIT_VIEW.md](docs/FIT_VIEW.md).
Bounded per-tab arrow-key canvas navigation is documented in
[docs/KEYBOARD_PAN.md](docs/KEYBOARD_PAN.md).
Deterministic keyboard resolution of modified documents during multi-tab quit
is documented in [docs/MULTI_TAB_QUIT.md](docs/MULTI_TAB_QUIT.md).
Bounded next/previous keyboard navigation in current visual document-tab order
is documented in
[docs/DOCUMENT_TAB_NAVIGATION.md](docs/DOCUMENT_TAB_NAVIGATION.md).
Keyboard Layers-panel visibility and direct panel/canvas focus are documented
in [docs/LAYERS_PANEL_FOCUS.md](docs/LAYERS_PANEL_FOCUS.md).
Bidirectional keyboard reorder for flat pixel layers and its metadata-only
resource boundary are documented in
[docs/LAYER_REORDER.md](docs/LAYER_REORDER.md).
The first Phase 1 typed-pixel and checked byte-layout contracts are documented
in [docs/PIXEL_STORAGE.md](docs/PIXEL_STORAGE.md).

## Arch package

The development package recipe lives at `packaging/arch/PKGBUILD` and builds directly from `main`:

```sh
cd packaging/arch
makepkg -si
```

That command intentionally follows the rolling Git branch and therefore is not
a reproducible-release claim. CI instead archives the exact checked-out SHA,
records it in the source tree, computes and enforces the archive BLAKE2 checksum,
runs package `check()`, verifies the package version, and installs the result.
The Arch container and GitHub action are digest/commit pinned, but Arch's rolling
repository packages are not yet snapshot-pinned; release reproducibility remains
Planned until a dated package repository/snapshot policy is adopted.

## Omarchy plugin

The root `manifest.json` is the marketplace contract. Once the native binary is installed on `PATH` as `chromarchy`:

```sh
omarchy plugin add https://github.com/rohan-patnaik/Chromarchy.git --enable
omarchy-shell shell summon io.github.rohan-patnaik.chromarchy '{}'
```

## License

MIT. Individual optional dependencies may have their own compatible terms.
