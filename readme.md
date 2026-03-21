# Build Guide (WSL + CMake)

This project is developed and tested with **WSL** and **CMake**.

## 1. Prerequisites

- WSL installed (Ubuntu or similar)
- `cmake` (3.15+)
- `g++` or `clang++` with C++23 support
- `make` (or Ninja if you prefer)

Install common tools on Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential cmake
```

## 2. Project Root

Use this directory as the project root:

```bash
/mnt/d/ftcl/ftcl
```

## 3. Configure

From WSL:

```bash
cd /mnt/d/ftcl/ftcl
cmake -S . -B build
```

## 4. Build

```bash
cmake --build build -j4
```

You can increase `-j4` depending on your CPU.

## 5. Run All Registered Tests

```bash
cd /mnt/d/ftcl/ftcl/build
ctest --output-on-failure
```

List tests without running:

```bash
ctest -N
```

## 6. Tcl Suite Tests

The CTest suite includes:

- `ftclSuiteTest` (runs `test/tests/all.tcl`)
- `ftclSubsetTest` (runs selected passing subset)

Run full Tcl suite directly:

```bash
cd /mnt/d/ftcl/ftcl
./build/test/test_ftcl_tcl_suite /mnt/d/ftcl/ftcl/test/tests/all.tcl
```

Run only expression tests directly:

```bash
./build/test/test_ftcl_tcl_suite /mnt/d/ftcl/ftcl/test/tests/expr.tcl
```

## 7. Clean Rebuild

```bash
cd /mnt/d/ftcl/ftcl
cmake --build build --clean-first -j4
```

Or remove the build directory and reconfigure:

```bash
rm -rf build
cmake -S . -B build
cmake --build build -j4
```

## 8. Optional: Run from Windows PowerShell

```powershell
wsl bash -lc 'cd /mnt/d/ftcl/ftcl && cmake --build build -j4 && cd build && ctest --output-on-failure'
```

