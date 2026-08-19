# Performance validation

Chromarchy performance work is measured with reproducible workloads before release thresholds are declared. Current results are comparative engineering signals, not cross-machine guarantees.

## Core benchmark workloads

`chromarchy_core_benchmarks` currently measures:

- CPU compositing of a 1024×1024 document across eight fully populated tiled
  pixel layers;
- copy-on-write mutation of one pixel in a shared 1024×1024 tiled layer;
- inversion of a 512×512 sparse selection in a 300,000×300,000 mask.

## Canvas benchmark workload

`chromarchy_canvas_benchmarks` measures viewport-bounded painting of a sparse
300,000×300,000 document at 1% zoom into a 640×480 widget.

The large canvas and selection workloads intentionally verify that cost follows
allocated or visible tiles rather than total canvas area.

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
