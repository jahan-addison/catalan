# cadence

## Overview

cadence is an opinionated boilerplate for modern C++ projects. It provides what I believe is essential tooling for code quality, testing, and structure for projects of any size. The build system integrates sanitizers, static analyzers, and automated formatting to catch errors, undefined behavior, and segmentation faults during all stages of development.

The boilerplate uses C++20 and includes a test suite built with `doctest`, along with pre-commit hooks for code quality enforcement via `clang-format`, `clang-tidy`, and `cppcheck`.

## Build

The recommended build command enables all available tooling:

```bash
cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DIWYU=ON
cmake --build build
```

For release builds, omit sanitizers and debug symbols:

```bash
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Windows

```bash
cmake -Bbuild -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

Or with `ninja`:

```bash
cmake -Bbuild -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

---

Run the test suite:

```bash
./build/test_suite
```

## Installation

### macOS

Install dependencies with Homebrew:

```bash
brew install cmake llvm cppcheck
brew install --HEAD include-what-you-use
```

Valgrind is not officially supported on modern macOS. Use sanitizers instead.

### Linux

For Debian/Ubuntu:

```bash
sudo apt-get install cmake clang-tidy clang-format cppcheck valgrind iwyu
```

For Fedora/RHEL:

```bash
sudo dnf install cmake clang-tools-extra cppcheck valgrind iwyu
```

### Windows

```bash
# Install MSYS2 from https://www.msys2.org/
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
pacman -S mingw-w64-x86_64-cppcheck mingw-w64-x86_64-clang-tools-extra
```

**Note**: Sanitizers, Valgrind, and IWYU have limited or no support on Windows.

#### MSVC

The CMakeLists.txt includes MSVC-specific flags (`/W4 /WX`).

## Tooling

### Sanitizers

The build system supports multiple sanitizers via `-DUSE_SANITIZER`. These are runtime instrumentation tools that detect bugs during program execution.

- **AddressSanitizer**: Detects memory errors like use-after-free, buffer overflows, and stack corruption
- **UndefinedBehaviorSanitizer**: Catches undefined behavior such as integer overflow, null pointer dereferences, and misaligned memory access
- **MemorySanitizer**: Detects uninitialized memory reads (requires special libc build)
- **ThreadSanitizer**: Detects data races in multithreaded code
- **LeakSanitizer**: Detects memory leaks

Sanitizers impose runtime overhead but catch bugs that static analysis tools miss. I personally **do not** recommend developing in C++ without at least `Address;Undefined`.

### Pre-commit Hooks

The `.pre-commit-config.yaml` defines git hooks that run automatically before commits:

- **conventional-pre-commit**: Enforces conventional commit message format
- **clang-format**: Formats code automatically
- **clang-tidy**: Runs static analysis
- **cppcheck**: Performs additional static checks

Install pre-commit hooks:

```bash
pip install pre-commit
pre-commit install
pre-commit install --hook-type commit-msg
```

### Valgrind

Valgrind is a dynamic analysis tool that detects memory management issues and profiling. The `memcheck` tool finds memory leaks, invalid memory access, and use of uninitialized memory. Unlike sanitizers, Valgrind requires no recompilation but runs significantly slower.

Run valgrind on the test suite:

```bash
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./build/test_suite
```

Valgrind is particularly useful on Linux platforms where it has mature support. The CI pipeline runs valgrind automatically on x86_64 Linux builds.

### clang-tidy

A static analysis tool that performs linting and suggests modernization improvements. It checks for common programming errors, style violations, and provides automated fixes. Configuration is in `.clang-tidy`, which scopes checks to `clang-diagnostic-*`, `clang-analyzer-*`, `bugprone-*`, `performance-*`, and `portability-*`, and restricts `HeaderFilterRegex` so third-party CPM dependencies aren't reported on.

Run the full project through `clang-tidy` with:

```bash
cmake --build build --target tidy
```

This target runs `run-clang-tidy` in parallel across all cores and treats warnings as errors, so it fails the build the same way a broken compile would. It also de-duplicates `compile_commands.json` first via `cmake/dedup_compile_commands.cmake` -- any source compiled into more than one target (a library and its test binary) would otherwise get linted once per target, doubling the runtime cost. Which files get linted is controlled by `CLANG_TIDY_IGNORE_REGEX`, a cache variable you can override per project (`test/` and CPM's cache are excluded by default).

The pre-commit hook (`.pre-commit-config.yaml`) still runs `clang-tidy` per-file on your staged changes, which is faster for a single commit; the CMake target is for linting everything, e.g. in CI.

### clang-format

An automated code formatter that enforces consistent style. Configuration is in `.clang-format` (create as needed). The pre-commit hook applies formatting automatically before commits.

Run manually:

```bash
clang-format -i cadence/**/*.{cc,h}
```

### cppcheck

A static analysis tool for C++ that detects bugs, undefined behavior, and dangerous coding constructs. It complements compiler warnings and clang-tidy by checking for issues both tools might miss.

Configuration is in `cppcheck.suppress` which excludes false positives and third-party dependencies. The tool runs via pre-commit hooks with:

```bash
cppcheck --enable=all --inline-suppr --error-exitcode=1 --language=c++ -I. --suppressions-list=./cppcheck.suppress
```

### Include What You Use (IWYU)

A tool that analyzes `#include` directives and suggests correct headers. Enable with `-DIWYU=ON` to run during compilation. It helps minimize header dependencies and reduce compilation time.

IWYU integrates directly into the build process and reports warnings during compilation. The tool is strict and may require manual tuning via pragmas.

### CMake Tools

The project uses `cmake-scripts` from StableCoder which provides additional CMake functionality:

- **Sanitizer support**: Easy sanitizer integration via `sanitizers.cmake`
- **Code coverage**: Generate coverage reports with LLVM or GCC
- **Compiler options**: Consolidated compiler warning flags

These are automatically fetched via CPM (CMake Package Manager) during configuration.

## Dependencies

Dependencies are managed via CPM and fetched automatically:

- `doctest` - Fast, header-only testing framework
- `fmt` - Fast, type-safe formatting library
- `matchit` - Pattern matching for C++
- `cmake-scripts` - Additional CMake utilities

To add dependencies, edit `CMakeLists.txt` and use `cpmaddpackage`.

## Project Structure

```
cadence/
├── cadence/          Source files
│   ├── hello.cc
│   ├── hello.h
│   └── main.cc
├── test/               Test files
│   ├── hello.cc
│   └── main.cc
├── cmake/              CMake modules
│   ├── CPM.cmake
│   └── doctest.cmake
├── CMakeLists.txt      Build configuration
├── .pre-commit-config.yaml
└── cppcheck.suppress   Cppcheck suppressions
```

## Extras

Notes, books on C++: https://soliloq.uy/my-cpp-notes-documents-books/

The license is CC0 1.0 Universal, i.e. "public domain." Replace with your own license.
