# Benchmark Guide

This guide explains how to run `bench_ftcl`, what each output means, and how to interpret the three benchmark charts in reports or thesis defense slides.

## Goal

The benchmark pipeline is designed to answer three different questions:

1. Correctness trend: Are language semantics still passing as features evolve?
2. Communication cost: How expensive is thread channel messaging?
3. Runtime smoothness: How stable is frame execution time (especially tail latency)?

## Environment Checklist

Record these details before sharing benchmark numbers:

- OS and kernel (`uname -a`)
- CPU model and core count (`lscpu`)
- Compiler and version (`g++ --version`)
- CMake version (`cmake --version`)
- Build type (`Release` strongly recommended)
- Commit hash (`git rev-parse HEAD`)
- Benchmark timestamp (UTC preferred)

Why this matters:

- CPU, kernel scheduler, and build mode can materially change latency tails (`P95`, `P99`).
- Without environment metadata, results are hard to compare or reproduce.

## Build

From project root (`/mnt/d/ftcl/ftcl`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

Executable path:

```text
build/test/bench_ftcl
```

## Run Benchmarks

Run and write CSV outputs to `docs/benchmark_data`:

```bash
./build/test/bench_ftcl ./docs/benchmark_data
```

The benchmark includes warmup and then records sample distributions.

## Output Files and Meaning

### `semantic_pass_rate.csv`

Columns:

- `suite`: semantic test group name.
- `total`: number of semantic checks in this suite.
- `passed`: number of checks that passed.
- `pass_rate_pct`: `passed / total * 100`.

Interpretation:

- This is a correctness signal, not a speed signal.
- A drop in pass rate means behavior regression, even if performance improves.

### `semantic_pass_rate_snapshot.csv`

Columns:

- `timestamp_utc`
- `total`
- `passed`
- `pass_rate_pct`

Interpretation:

- Single run summary for time-series tracking across commits.
- Useful for plotting "semantic quality over time".

### `channel_latency_us.csv`

Columns:

- `sample_idx`
- `one_way_latency_us`

Definition:

- Measured from ping-pong round-trip between two channels.
- One-way estimate is `RTT / 2`.

Interpretation:

- Lower is better.
- Look at spread, not only the average.

### `channel_latency_summary.csv`

Columns:

- `min_us`, `max_us`, `mean_us`, `p50_us`, `p95_us`, `p99_us`

Interpretation:

- `P50`: typical case.
- `P95`: stress-but-common tail.
- `P99`: rare slow path, sensitive to scheduler noise and contention.

### `frame_time_us.csv`

Columns:

- `sample_idx`
- `frame_time_us`

Definition:

- Time for one benchmark frame step in non-interactive simulation.

Interpretation:

- For game/runtime responsiveness, tail (`P95/P99`) is often more important than mean.

### `frame_time_summary.csv`

Columns:

- `min_us`, `max_us`, `mean_us`, `p50_us`, `p95_us`, `p99_us`

Interpretation:

- Use this as the headline frame stability table in the thesis.

## Generate Figures

Generate SVG figures from CSV (Python standard library only):

```bash
python3 ./docs/plot_benchmarks.py ./docs/benchmark_data ./docs/figures
```

Generated files:

- `docs/figures/semantic_pass_rate.svg`
- `docs/figures/channel_latency_distribution.svg`
- `docs/figures/frame_time_distribution.svg`

## How to Read the Three Figures

### 1) Semantic Pass Rate by Suite

What it shows:

- Per-suite correctness percentages.

How to discuss it:

- "All core suites remain at 100%, except suite X at Y%."
- "This indicates semantic alignment progress/regression independent of speed."

### 2) Channel Latency Distribution

What it shows:

- Histogram of one-way channel latency samples.
- Vertical markers for `P50`, `P95`, `P99`.

How to discuss it:

- `P50` describes normal messaging overhead.
- `P95/P99` show tail behavior under jitter/contention.
- A small `P50` with very large `P99` means average looks good but occasional spikes exist.

### 3) Frame Time Distribution

What it shows:

- Histogram of frame-step durations.
- Vertical markers for `P50`, `P95`, `P99`.

How to discuss it:

- `P50` reflects normal smoothness.
- `P95/P99` correlate with visible stutter risk.
- A lower `P99` is often more meaningful for user experience than a lower mean.

## Percentiles: Practical Definition

- `P50`: 50% of samples are less than or equal to this value.
- `P95`: 95% of samples are less than or equal to this value.
- `P99`: 99% of samples are less than or equal to this value.

Rule of thumb:

- Use `P50` for "typical latency".
- Use `P95/P99` for "worst-case user-visible risk".

## Optional Perf Profiling

Use `perf` when you need *why* something is slow (hotspot attribution), not just *how slow*:

```bash
perf stat -r 10 ./build/test/bench_ftcl ./docs/benchmark_data
perf record -g -- ./build/test/bench_ftcl ./docs/benchmark_data
perf report
```

Recommended workflow:

1. Use benchmark CSV to detect regression (`P95/P99` increase).
2. Use `perf` to locate hot functions and call paths.
3. Optimize.
4. Re-run benchmark and compare the same metrics.

## Suggested Reporting Template

When reporting one run, include:

- Environment block (CPU, compiler, build type).
- Semantic pass-rate table.
- Channel latency (`P50/P95/P99`).
- Frame time (`P50/P95/P99`).
- The three SVG figures.
- Commit hash and benchmark timestamp.

## Reproducibility Tips

- Run on an idle machine when possible.
- Run at least 3 times; compare variance of `P95/P99`.
- Keep benchmark parameters unchanged between commits.
- If numbers differ significantly, note system load and thermal conditions.
