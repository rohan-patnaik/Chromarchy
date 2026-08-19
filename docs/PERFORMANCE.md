# Performance validation

Chromarchy performance work is measured with reproducible workloads before release thresholds are declared. Current results are comparative engineering signals, not cross-machine guarantees.

## Core benchmark workloads

`chromarchy_core_benchmarks` currently measures:

- CPU compositing of a visible 1024×1024 region across eight tiled pixel layers in a sparse 20,000×20,000 document;
- copy-on-write mutation of one pixel in a shared tile set;
- inversion of a 512×512 sparse selection in a 300,000×300,000 document.
- viewport-bounded painting of a sparse 300,000×300,000 document at 1% zoom.

The large canvas and selection workloads intentionally verify that cost follows allocated or visible tiles rather than total canvas area.

## Run locally

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
./build/chromarchy_core_benchmarks
```

Run only benchmark-labeled CTest entries with:

```sh
ctest --test-dir build -L benchmark --output-on-failure
```

CI builds and executes the benchmark on current Arch Linux to catch crashes and pathological regressions. Stable latency and memory gates will be added after measurements are collected on the target Omarchy Quattro hardware.
