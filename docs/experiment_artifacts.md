# FTCL Experiment Artifacts Guide

This document explains where the experiment scripts, CSV data, and SVG figures are stored, how to regenerate them, and what each figure is intended to show in the thesis.

## Directory Layout

```text
docs/
  benchmark.md                         General benchmark guide
  experiment_artifacts.md              This guide
  scripts/
    benchmarks/
      plot_benchmarks.py               Draw core benchmark figures from CSV
      benchmark_parser_backends.py     Measure legacy vs token_stream parser runtime
    geometry/
      generate_geometry_figures.py     Generate geometry/UVec architecture and result figures
      README.md                        Short geometry script usage note
  data/
    benchmarks/                        Core benchmark CSV files
    geometry/                          Geometry benchmark and CPU/CUDA error CSV files
  figures/
    benchmarks/                        Language/runtime benchmark figures
    architecture/                      System architecture and UVec design figures
    geometry/                          Geometry algorithm and CPU/CUDA result figures
```

## Build Before Running Scripts

CPU build:

```bash
cmake -S . -B /tmp/ftcl-build-geometry-cpu \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DFTCL_ENABLE_CUDA=OFF \
  -DFTCL_CXX_STANDARD=20
cmake --build /tmp/ftcl-build-geometry-cpu -j8
```

CUDA build:

```bash
cmake -S . -B /tmp/ftcl-build-geometry-cuda \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DFTCL_ENABLE_CUDA=ON \
  -DFTCL_CXX_STANDARD=20
cmake --build /tmp/ftcl-build-geometry-cuda -j8
```

## Scripts

### `docs/scripts/benchmarks/plot_benchmarks.py`

Purpose:

- Reads existing core benchmark CSV files.
- Draws semantic pass-rate and latency distribution SVG figures.
- Uses only the Python standard library.

Default usage:

```bash
python3 docs/scripts/benchmarks/plot_benchmarks.py
```

Explicit usage:

```bash
python3 docs/scripts/benchmarks/plot_benchmarks.py \
  docs/data/benchmarks \
  docs/figures/benchmarks
```

Input data:

- `docs/data/benchmarks/semantic_pass_rate.csv`
- `docs/data/benchmarks/channel_latency_us.csv`
- `docs/data/benchmarks/channel_latency_summary.csv`
- `docs/data/benchmarks/frame_time_us.csv`
- `docs/data/benchmarks/frame_time_summary.csv`

Generated figures:

- `docs/figures/benchmarks/semantic_pass_rate.svg`
- `docs/figures/benchmarks/channel_latency_distribution.svg`
- `docs/figures/benchmarks/frame_time_distribution.svg`

### `docs/scripts/benchmarks/benchmark_parser_backends.py`

Purpose:

- Runs the same Tcl subset test under two parser backends.
- Compares `legacy` and `token_stream` runtime.
- Writes raw samples, summary statistics, and a paper-ready SVG chart.

Usage:

```bash
python3 docs/scripts/benchmarks/benchmark_parser_backends.py \
  --binary /tmp/ftcl-build-geometry-cpu/test/test_ftcl_subset \
  --tests-dir ./test/tests \
  --rounds 24 \
  --warmup 4
```

Generated data:

- `docs/data/benchmarks/parser_backend_timing_raw.csv`
- `docs/data/benchmarks/parser_backend_timing_summary.csv`

Generated figure:

- `docs/figures/benchmarks/parser_backend_timing.svg`

### `docs/scripts/geometry/generate_geometry_figures.py`

Purpose:

- Runs geometry correctness/performance test executables from the CUDA build tree.
- Extracts CPU/CUDA end-to-end timing data.
- Extracts CPU/CUDA numerical error samples.
- Draws architecture, UVec synchronization, command-flow, speedup, throughput, timing, error, and algorithm visualization figures.

Usage:

```bash
python3 docs/scripts/geometry/generate_geometry_figures.py \
  --build-dir /tmp/ftcl-build-geometry-cuda
```

Generated data:

- `docs/data/geometry/geometry_perf_scale.csv`
- `docs/data/geometry/geometry_error_samples.csv`

Generated architecture figures:

- `docs/figures/architecture/ftcl_geometry_system_architecture.svg`
- `docs/figures/architecture/uvec_cross_device_sync.svg`
- `docs/figures/architecture/geom_command_execution_flow.svg`

Generated geometry figures:

- `docs/figures/geometry/geometry_cpu_cuda_speedup.svg`
- `docs/figures/geometry/geometry_execution_time_bars.svg`
- `docs/figures/geometry/geometry_throughput_curve.svg`
- `docs/figures/geometry/geometry_error_distribution.svg`
- `docs/figures/geometry/geometry_algorithm_visualization.svg`

## Data Files

### Benchmark Data

`semantic_pass_rate.csv`

- Shows how many semantic tests pass in each language feature suite.
- Used to support the claim that FTCL remains correct while new features are added.

`semantic_pass_rate_snapshot.csv`

- Stores a one-line timestamped correctness snapshot.
- Useful for tracking correctness across commits.

`channel_latency_us.csv`

- Stores per-sample one-way `thread channel` latency.
- The one-way latency is estimated as half of ping-pong round-trip time.

`channel_latency_summary.csv`

- Stores summary statistics such as min, mean, P50, P95, and P99.
- Useful for discussing tail latency.

`frame_time_us.csv`

- Stores per-frame execution time for a non-interactive game-like workload.
- Useful for discussing runtime smoothness.

`frame_time_summary.csv`

- Stores P50/P95/P99 frame time statistics.
- Useful for reporting whether frame time is stable.

`parser_backend_timing_raw.csv`

- Stores every timing sample for parser backend comparison.
- Useful for reproducibility and variance inspection.

`parser_backend_timing_summary.csv`

- Stores mean, median, P95, standard deviation, and 95% confidence interval.
- Useful for a compact thesis table.

### Geometry Data

`geometry_perf_scale.csv`

- Stores end-to-end timing for `batch_distance_matrix`, `nearest_point`, and `range_count_circle`.
- Includes CPU and CUDA rows.
- Timing uses `FTCL_GEOMETRY_PERF_MODE=paper` by default in the geometry figure script.
- Inputs are prebuilt before timing; each row reports median command time.
- Timing includes UVec handle lookup, output allocation, synchronization, and backend computation.

`geometry_error_samples.csv`

- Stores max absolute CPU/CUDA error for geometry equivalence checks.
- Used to show that CUDA acceleration preserves numerical correctness.

## Figure Explanations

### Benchmark Figures

`semantic_pass_rate.svg`

- Shows semantic pass rate by feature suite.
- Use it in the correctness evaluation chapter.
- A higher bar means the interpreter behavior is closer to the intended Tcl subset semantics.

`channel_latency_distribution.svg`

- Shows the distribution of one-way `thread channel` latency.
- Includes P50/P95/P99 markers.
- Use it to explain concurrency communication cost and tail latency.

`frame_time_distribution.svg`

- Shows the distribution of frame-step execution time.
- Includes P50/P95/P99 markers.
- Use it to discuss whether FTCL can support interactive scripts smoothly.

`parser_backend_timing.svg`

- Compares `legacy` and `token_stream` parser backend runtime.
- Use it to discuss parser architecture tradeoffs.
- The current `token_stream` path may be slower because it performs extra tokenization and carries larger constant factors.

### Architecture Figures

`ftcl_geometry_system_architecture.svg`

- Shows the full path from FTCL script to parser/interpreter, `geom` command, UVec, and CPU/CUDA backends.
- This is suitable as the first core system architecture figure in the thesis.

`uvec_cross_device_sync.svg`

- Shows CPU buffer, CUDA buffer, valid flags, and automatic CPU-to-CUDA / CUDA-to-CPU synchronization.
- This figure explains the main systems-design contribution of the project.

`geom_command_execution_flow.svg`

- Shows how a command such as `geom nearest_point $dataset $queries cuda:0` is executed.
- It explains argument parsing, UVec handle lookup, backend selection, and result handle creation.

### Geometry Figures

`geometry_cpu_cuda_speedup.svg`

- Shows CPU time divided by CUDA time across input sizes.
- A value above 1 means CUDA is faster end-to-end.
- Because the measurement still includes output allocation, launch, and synchronization overhead, small or middle-sized inputs may not always show acceleration. Large batch workloads are where CUDA should win.

`geometry_execution_time_bars.svg`

- Compares CPU and CUDA execution time at the largest measured scale.
- This is easier to read than a table in a presentation slide.

`geometry_throughput_curve.svg`

- Shows operations per second for each algorithm and device.
- This is useful for explaining scalability and backend efficiency.

`geometry_error_distribution.svg`

- Shows numerical difference between CPU and CUDA outputs.
- This supports the correctness claim for CUDA acceleration.

`geometry_algorithm_visualization.svg`

- Visually explains representative geometry algorithms: point-in-polygon, nearest point, circle range query, and AABB collision.
- This is useful for non-specialist readers and thesis defense slides.

## Recommended Reproduction Workflow

```bash
# 1. Build CPU and CUDA test trees.
cmake --build /tmp/ftcl-build-geometry-cpu -j8
cmake --build /tmp/ftcl-build-geometry-cuda -j8

# 2. Regenerate core benchmark CSV data.
/tmp/ftcl-build-geometry-cpu/test/bench_ftcl docs/data/benchmarks

# 3. Regenerate benchmark figures.
python3 docs/scripts/benchmarks/plot_benchmarks.py

# 4. Regenerate parser backend timing data and figure.
python3 docs/scripts/benchmarks/benchmark_parser_backends.py \
  --binary /tmp/ftcl-build-geometry-cpu/test/test_ftcl_subset \
  --tests-dir ./test/tests \
  --rounds 24 \
  --warmup 4

# 5. Regenerate geometry data and figures.
python3 docs/scripts/geometry/generate_geometry_figures.py \
  --build-dir /tmp/ftcl-build-geometry-cuda
```

## Notes for Thesis Writing

- Use architecture figures in the design chapter.
- Use semantic pass rate and CPU/CUDA equivalence data in the correctness chapter.
- Use speedup, throughput, and timing figures in the performance chapter.
- Mention that geometry timings are end-to-end FTCL command timings, not pure kernel-only timings.
- For small inputs, CUDA may be slower because launch and synchronization overhead dominate.
## Geometry Add-on Study Artifacts

### Data

`geometry_break_even_points.csv`

- Stores per-algorithm CUDA break-even scale estimates (speedup crossing 1.0).
- Useful for explaining why small workloads may remain CPU-faster.

`geometry_ablation.csv`

- Raw ablation measurements for inline literal path, prebuilt handle path, and prebuilt+readback path.
- Used to separate parsing/creation overhead from pure command execution cost.

`geometry_ablation_table.md`

- Paper-ready markdown table summarizing median latency, inline-to-prebuilt gain, and readback overhead.

`geometry_concurrency.csv`

- Worker-count sweep for thread-channel geometry workload.
- Includes throughput and P50/P95/P99 one-request latency under pipelined load.

`geometry_multi_gpu_scaling.csv`

- Weak-scaling measurements for 1, 2, 4, and 8 CUDA devices.
- Each GPU receives the same number of query points; total work grows with GPU count.
- Includes wall-clock command time, throughput, speedup versus one GPU, and parallel efficiency.
- Requires all target devices to be visible through `CUDA_VISIBLE_DEVICES`.

### Figures

`geometry_break_even_point.svg`

- Shows speedup curves with 1.0 reference to highlight CUDA break-even behavior.

`geometry_real_application_demo.svg`

- Presents an ocean-oriented spatial computing scene and matching FTCL script snippet.
- The scene maps marine ranch sensors, floating objects, sonar/radar coverage, port obstacles, and ship collision checks to FTCL geometry commands.

`geometry_concurrency_throughput.svg`

- Throughput versus worker count for concurrent geometry tasks.

`geometry_concurrency_latency_p95.svg`

- P95 latency versus worker count for the same concurrency experiment.

`geometry_multi_gpu_scaling.svg`

- Shows throughput speedup for the multi-GPU weak-scaling experiment.
- Use it to demonstrate whether FTCL can actually use the eight A800 GPUs rather than only one CUDA device.
- A near-linear curve means the algorithm has enough independent work and compact outputs; a flat curve suggests command overhead, allocation, synchronization, or output bandwidth bottlenecks.

`uvec_state_machine.svg`

- Explicit state machine view of CPU-valid / CUDA-valid / both-valid transitions for UVec.

### Reproduction

```bash
python3 docs/scripts/geometry/generate_geometry_study_addons.py \
  --build-dir /tmp/ftcl-build-geometry-cuda
```

For the multi-GPU server run:

```bash
CUDA_VISIBLE_DEVICES=0,1,2,3,4,5,6,7 \
python3 docs/scripts/geometry/generate_geometry_study_addons.py \
  --build-dir /tmp/ftcl-build-geometry-cuda \
  --multi-gpu-mode paper \
  --multi-gpu-scaling weak
```
