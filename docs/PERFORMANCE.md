# Performance validation

Chromarchy performance work is measured with reproducible workloads before release thresholds are declared. Current results are comparative engineering signals, not cross-machine guarantees.

## Core benchmark workloads

`chromarchy_core_benchmarks` currently measures:

- CPU compositing of a 1024×1024 document across eight fully populated tiled
  pixel layers;
- copy-on-write mutation of one pixel in a shared 1024×1024 tiled layer;
- inversion of a 512×512 sparse selection in a 300,000×300,000 mask; and
- materialization of a 512×512 unsigned-16 straight-RGBA sparse region into
  premultiplied RGBA8.

## Canvas benchmark workload

`chromarchy_canvas_benchmarks` measures viewport-bounded painting of a sparse
300,000×300,000 document at 1% zoom into a 640×480 widget at both zero and 90
degrees of non-destructive view rotation. It also measures the display-only
pixel grid at 3200% zoom on the same sparse extent; the 640×480 fixture submits
at most 40 visible horizontal-plus-vertical boundary lines. A constant-space
fit benchmark repeatedly moves the same 300k-square sparse canvas from maximum
zoom to the existing 1% minimum clamp without scanning document pixels.

The large canvas and selection workloads intentionally verify that cost follows
allocated or visible tiles rather than total canvas area.

Checked pixel byte geometry is validated independently of allocation. Current
RGBA8 tiles remain 256×256 (262,144 payload bytes); the typed-format contract
allocates high-depth tiles only in isolated tests under 1-MiB per-tile and
16-MiB/64-tile store, region, and combined delta-payload bounds. Checked
unsigned 8/16-bit channel conversion has a caller-bounded RGBA8 destination and
never allocates an implicit full frame. The typed-region benchmark exercises
that conversion with one combined source-plus-destination cap; it does not
change the live RGBA8 renderer.

## Run locally

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
./build/chromarchy_core_benchmarks
QT_QPA_PLATFORM=offscreen ./build/chromarchy_canvas_benchmarks
```

Run only benchmark-labeled CTest entries with:

```sh
ctest --test-dir build -L benchmark --output-on-failure
```

CI builds and executes the benchmark on current Arch Linux to catch crashes and pathological regressions. Stable latency and memory gates will be added after measurements are collected on the target Omarchy Quattro hardware.

## Deterministic reference fixture

`tests/fixtures/composite-source-over-golden.json` defines a complete 257×2
source-over result from an independent exact-rational implementation. It covers
fractional opacity, alpha endpoints, a hidden layer, ordering, and the x=255/256
tile boundary. The fixture digest authenticates every row-major RGBA8 output
byte; the document test checks its documented compounded 8-bit rounding bound,
byte-identical repeat and region/painter behavior, and native persistence for
the layered, flattened, and merge-down results. This is an RGBA8 reference
contract, not a color-managed or extended-blend-mode claim.
