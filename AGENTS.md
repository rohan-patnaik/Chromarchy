# Project instructions

- Target Omarchy Quattro on current Arch Linux and Wayland first.
- Keep the root `manifest.json` and `Plugin.qml` valid as an Omarchy plugin.
- Keep the native app independently runnable; the shell plugin is a thin launcher/integration surface.
- Prefer C++23, Qt 6, RAII, value types, and explicit ownership. Avoid raw owning pointers.
- Preserve an offline-first design. Do not add telemetry or mandatory cloud services.
- Do not copy Adobe code, icons, strings, layouts, sample files, or other expressive assets.
- Implement original visuals and terminology while supporting standard file formats and keyboard conventions.
- Add focused tests with each feature and commit/push coherent feature slices to `main`.
- Do not claim Photoshop parity unless the tracked parity matrix and tests substantiate it.

