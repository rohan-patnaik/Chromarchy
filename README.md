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

## Arch package

The development package recipe lives at `packaging/arch/PKGBUILD` and builds directly from `main`:

```sh
cd packaging/arch
makepkg -si
```

## Omarchy plugin

The root `manifest.json` is the marketplace contract. Once the native binary is installed on `PATH` as `chromarchy`:

```sh
omarchy plugin add https://github.com/rohan-patnaik/Chromarchy.git --enable
omarchy-shell shell summon io.github.rohan-patnaik.chromarchy '{}'
```

## License

MIT. Individual optional dependencies may have their own compatible terms.

