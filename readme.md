# FTCL (C++ Tcl-like Interpreter)

FTCL is a C++23 header-only Tcl-like interpreter, developed and validated in WSL with CMake.

## Highlights

- Header-only runtime in `include/`
- Tcl-style language core (`eval`, `proc`, scope, list/dict/string/array)
- `expr` support including bitwise, shifts, ternary, short-circuit, and overflow checks
- Concurrency primitives:
  - `thread spawn`
  - `thread await`
  - `thread channel create/send/recv/try_recv`
- Input commands including `getch` (`-noblock`) and `gets`
- Tcl script harness in `test/tests/*.tcl`
- Non-interactive benchmark binary: `bench_ftcl`

## Environment

Recommended:

- WSL (Ubuntu)
- CMake >= 3.15
- GCC/Clang with C++23 support

Install dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake python3
```

Project root in WSL:

```bash
/mnt/d/ftcl/ftcl
```

## Quick Start (Build + Test)

```bash
cd /mnt/d/ftcl/ftcl
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Expected result should be all tests passed.

From Windows PowerShell (without entering WSL shell manually):

```powershell
wsl --cd /mnt/d/ftcl/ftcl cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
wsl --cd /mnt/d/ftcl/ftcl cmake --build build -j4
wsl --cd /mnt/d/ftcl/ftcl/build ctest --output-on-failure
```

## Build Options

Parser backend compile-time default:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFTCL_PARSER_DEFAULT_BACKEND=0
```

Values:

- `0`: legacy parser
- `1`: token_stream parser
- `2`: shadow compare (legacy vs token_stream)

You can also override at runtime:

```bash
FTCL_PARSER_BACKEND=legacy ./build/test/test_ftcl_subset ./test/tests
FTCL_PARSER_BACKEND=token_stream ./build/test/test_ftcl_subset ./test/tests
FTCL_PARSER_BACKEND=shadow ./build/test/test_ftcl_subset ./test/tests
```

## Test Commands

List tests:

```bash
ctest --test-dir build -N
```

Run all:

```bash
ctest --test-dir build --output-on-failure
```

Re-run only failed tests:

```bash
ctest --test-dir build --rerun-failed --output-on-failure
```

Run one test group:

```bash
ctest --test-dir build -R '^ExprSemanticsTest$' --output-on-failure
```

## Tcl Suite

Run full suite:

```bash
./build/test/test_ftcl_suite ./test/tests/all.tcl
```

Run one Tcl file:

```bash
./build/test/test_ftcl_suite ./test/tests/expr.tcl
```

Run subset:

```bash
./build/test/test_ftcl_subset ./test/tests
```

## Benchmark

Run core benchmark and generate figures:

```bash
./build/test/bench_ftcl ./docs/benchmark_data
python3 ./docs/plot_benchmarks.py ./docs/benchmark_data ./docs/figures
```

Parser backend timing comparison (`legacy` vs `token_stream`):

```bash
python3 ./docs/benchmark_parser_backends.py \
  --binary ./build/test/test_ftcl_subset \
  --tests-dir ./test/tests \
  --rounds 24 \
  --warmup 4
```

Outputs:

- `docs/benchmark_data/parser_backend_timing_raw.csv`
- `docs/benchmark_data/parser_backend_timing_summary.csv`
- `docs/figures/parser_backend_timing.svg`

Detailed benchmark interpretation:

- `docs/benchmark.md`

## Real-Time Demo

```bash
./build/test/test_ftcl_suite ./game.tcl
```

Controls:

- `W/A/S/D`: move
- `F`: fire
- `Q`: quit

## Repository Layout

- `include/`: interpreter/runtime headers
- `test/`: C++ tests and benchmark executables
- `test/tests/`: Tcl test scripts
- `docs/benchmark.md`: benchmark definitions and metric interpretation
- `docs/benchmark_data/`: generated CSV data
- `docs/figures/`: generated SVG figures
- `game.tcl`: real-time game demo


In your figure, `token_stream` is slower than `legacy` not mainly because of a different theoretical complexity class, but because the current implementation has a larger constant factor and a few code paths that can degrade toward quadratic behavior.

Main reasons:

- `token_stream` performs an extra full lexing pass and copies the string for every token.  
  See `lexer.hpp` (line 148) (`tokenize_all`) and `lexer.hpp` (line 216) (string copy in `make_token`).

- Whitespace is now "one token per character", so the token count increases significantly.  
  See `lexer.hpp` (line 228).

- Variable resolution does repeated scanning / repeated lexing:

  - Linear search over all tokens: `parser.hpp` (line 890)  
  - Array indices rebuild `ParserTokenStreamAdapter` and re-lex: `parser.hpp` (line 1130)  
  - `[]` sub-scripts slice to a string and recursively go through `token_stream` again.  
    See `parser.hpp` (line 1074).

- Every backslash substitution creates a temporary `Tokenizer`.  
  See `parser.hpp` (line 861).

So the result you see (roughly several percent slower for `token_stream`) matches the current implementation.  
Conclusion: for most ordinary scripts both backends are close to linear, but `token_stream` has a larger constant factor, and the above points can push it toward near-quadratic behavior on certain inputs.