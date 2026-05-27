# Geometry figure scripts

This directory contains reproducible scripts for the FTCL geometry chapter figures.

Run from the repository root after building the CUDA test tree:

```bash
python3 docs/scripts/geometry/generate_geometry_figures.py --build-dir /tmp/ftcl-build-geometry-cuda
```

Outputs:

- Data CSV files: `docs/data/geometry/`
- Architecture SVG figures: `docs/figures/architecture/`
- Geometry SVG figures: `docs/figures/geometry/`

The performance curves use paper mode by default: inputs are prebuilt, CUDA is warmed up, and each row reports median end-to-end FTCL `geom` command time. The timed section includes UVec handle lookup, output allocation, backend execution, and synchronization, but excludes parsing the large input point literals.

For the full script/data/figure inventory, see `docs/experiment_artifacts.md`.

## Interpreting CUDA speedups

Small inputs may still run faster on CPU because CUDA has fixed costs: device memory preparation, kernel launch, synchronization, and result readback. The benchmark therefore reports several scales. A CUDA backend is considered useful when the speedup crosses 1.0 as the point/query count grows and continues to improve for large parallel workloads.

The current paper-mode benchmark is designed to measure the geometry command path rather than script parsing. It excludes large point-list parsing from the timed region, but still includes UVec handle lookup, output allocation, backend execution, and synchronization. This makes the result closer to what a real FTCL script pays after data has already been loaded into UVec.

## Add-on study bundle

To generate the break-even chart, ablation table, ocean spatial-computing demo scene, UVec state machine, and concurrency experiment charts:

```bash
python3 docs/scripts/geometry/generate_geometry_study_addons.py --build-dir /tmp/ftcl-build-geometry-cuda
```

Additional outputs:

- `docs/data/geometry/geometry_break_even_points.csv`
- `docs/data/geometry/geometry_ablation.csv`
- `docs/data/geometry/geometry_ablation_table.md`
- `docs/data/geometry/geometry_concurrency.csv`
- `docs/data/geometry/geometry_multi_gpu_scaling.csv`
- `docs/figures/geometry/geometry_break_even_point.svg`
- `docs/figures/geometry/geometry_real_application_demo.svg`
- `docs/figures/geometry/geometry_concurrency_throughput.svg`
- `docs/figures/geometry/geometry_concurrency_latency_p95.svg`
- `docs/figures/geometry/geometry_multi_gpu_scaling.svg`
- `docs/figures/architecture/uvec_state_machine.svg`

For the 8-GPU server experiment, expose all devices explicitly before running the add-on script:

```bash
CUDA_VISIBLE_DEVICES=0,1,2,3,4,5,6,7 \
python3 docs/scripts/geometry/generate_geometry_study_addons.py \
  --build-dir /tmp/ftcl-build-geometry-cuda \
  --multi-gpu-mode paper \
  --multi-gpu-scaling weak
```
