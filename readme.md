# FTCL (C++ Tcl-like Interpreter)

FTCL is a C++23, header-only Tcl-like interpreter project focused on semantic alignment with Molt while keeping development practical in WSL + CMake.

## Highlights

- Header-only interpreter/library (`include/`)
- Core Tcl-style language features (eval, proc, scope, list/dict/string/array basics)
- `expr` coverage including bitwise ops, shifts, ternary, short-circuit logic, and overflow checks
- Concurrency primitives:
  - `thread spawn`
  - `thread await`
  - `thread channel create/send/recv/try_recv`
- Real-time keyboard input with `getch` (`-noblock` supported)
- Test harness for Tcl test scripts (`test/tests/*.tcl`)
- Non-interactive benchmarking executable (`bench_ftcl`)

## Environment

Recommended environment:

- WSL (Ubuntu)
- CMake 3.15+
- GCC/Clang with C++23 support

Install common tools:

```bash
sudo apt update
sudo apt install -y build-essential cmake
```

Project root:

```bash
/mnt/d/ftcl/ftcl
```

## Build

```bash
cd /mnt/d/ftcl/ftcl
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

## Run Tests

Run all registered tests:

```bash
ctest --test-dir build/test --output-on-failure
```

List tests:

```bash
ctest --test-dir build/test -N
```

Run a specific test group (example):

```bash
ctest --test-dir build/test -R ExprSemanticsTest --output-on-failure
```

## Run Tcl Test Scripts

Run full Tcl suite:

```bash
./build/test/test_ftcl_suite ./test/tests/all.tcl
```

Run a single Tcl file:

```bash
./build/test/test_ftcl_suite ./test/tests/expr.tcl
```

Run passing subset runner:

```bash
./build/test/test_ftcl_subset ./test/tests
```

## Real-Time Game Demo

Run:

```bash
./build/test/test_ftcl_suite ./game.tcl
```

Controls:

- `W/A/S/D`: move
- `F`: fire
- `Q`: quit

The demo uses real-time `getch -noblock`, autonomous enemies, bullet-wall interaction, and thread-based concurrent updates.

## Benchmark

Run benchmark:

```bash
./build/test/bench_ftcl ./docs/benchmark_data
```

Generate figures:

```bash
python3 ./docs/plot_benchmarks.py ./docs/benchmark_data ./docs/figures
```

See detailed benchmark interpretation:

- `docs/benchmark.md`

## Repository Layout

- `include/`: FTCL headers (interpreter, commands, parser, expr, runtime)
- `test/`: C++ semantic tests, Tcl harness, benchmark executable
- `test/tests/`: Tcl test files
- `docs/benchmark.md`: benchmark definitions and interpretation guide
- `docs/benchmark_data/`: generated CSV metrics
- `docs/figures/`: generated benchmark SVG charts
- `game.tcl`: real-time demo script

## Notes

- Development and validation are performed in WSL.
- The project currently prioritizes semantic correctness and regression coverage.
