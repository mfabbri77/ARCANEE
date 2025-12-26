<!-- cmake_playbook.md -->

# CMake Playbook (Normative)

This playbook defines **standard CMake practices**, presets requirements, target conventions, and reproducibility expectations for repositories governed by the DET CDD blueprint system.

It is **normative** unless superseded by a higher-precedence rule via DEC + enforcement.

---

## 1. Goals

- Deterministic, reproducible builds across ubuntu/windows/macos.
- Single-source configuration via `CMakePresets.json`.
- Minimal “it works on my machine” variance.
- Clear mapping from components to targets.
- CI and local development share the same commands.

---

## 2. Baseline requirements (MUST)

### 2.1 Minimum CMake version

- The project MUST pin a minimum CMake version in `CMakeLists.txt`.
- Default: **CMake 3.28+** unless constraints require older (DEC required).

### 2.2 Language standard

- Default: **C++20**.
- MUST set `CMAKE_CXX_STANDARD` and `CMAKE_CXX_STANDARD_REQUIRED ON`.
- Deviations (C++17 or C++23) require DEC and CI confirmation across platforms.

### 2.3 Out-of-source builds

- MUST support out-of-source builds.
- In-source builds SHOULD be discouraged (optional guard).

### 2.4 Presets required (XPLAT-02)

- `CMakePresets.json` is REQUIRED.
- Presets MUST cover configure/build/test for:
  - linux
  - windows
  - macos
- Presets MUST include CI-oriented presets:
  - `ci-linux`, `ci-windows`, `ci-macos` (or equivalent naming, DEC if different)

### 2.5 Deterministic flags and hygiene

Builds MUST be deterministic under identical inputs:
- pin generator (Ninja recommended; MSBuild allowed via Windows presets)
- avoid embedding timestamps unless explicitly controlled
- use consistent warning levels per compiler

---

## 3. Project structure pattern (recommended)

Suggested structure:

- root `CMakeLists.txt` declares project, options, global settings
- `/src` contains per-module CMakeLists or is added via target_sources
- `/tests` contains test CMakeLists

Use:
- `add_library(lib_core ...)`
- `add_executable(app_...)`, `add_executable(tool_...)`, etc.

---

## 4. Required CMake options (MUST)

Projects MUST provide these cache options (project prefix is required, see repo_layout_and_targets.md):

- `<PROJ>_BUILD_TESTS` (default ON in CI; MAY default OFF locally)
- `<PROJ>_BUILD_EXAMPLES` (default OFF)
- `<PROJ>_ENABLE_SANITIZERS` (default OFF; CI enables as needed)
- `<PROJ>_ENABLE_WARNINGS_AS_ERRORS` (default ON in CI)
- `<PROJ>_ENABLE_LTO` (default OFF; optional)

The project prefix MUST be derived from `project(<name>)` and documented in Ch7.

---

## 5. Compiler warnings and errors (normative)

### 5.1 Warning levels

The project MUST set strict warnings per compiler:

- GCC/Clang: `-Wall -Wextra -Wpedantic`
- MSVC: `/W4`

Additional recommended warnings may be added, but avoid toolchain-breaking noise.

### 5.2 Warnings-as-errors

- In CI, warnings-as-errors MUST be enabled unless DEC says otherwise.
- Locally, it MAY be optional but should match CI by default.

---

## 6. Sanitizers and instrumentation (normative)

### 6.1 Linux/macOS

If `<PROJ>_ENABLE_SANITIZERS=ON`:
- Use:
  - AddressSanitizer (ASan)
  - UndefinedBehaviorSanitizer (UBSan)
- TSan SHOULD be available via a separate preset or option if feasible.

### 6.2 Windows

Windows sanitizer support is toolchain-dependent:
- Prefer Windows ASan if supported.
- Otherwise, use MSVC `/analyze` or clang-tidy as a substitute.
- Any limitation MUST be recorded as DEC, and CI MUST have an analysis job.

---

## 7. Testing integration (normative)

### 7.1 CTest integration required

- The project MUST use `enable_testing()` and register tests with `add_test()`, OR provide an equivalent `test_all` target (DEC required if not using CTest).
- Prefer `ctest` invocation via presets.

### 7.2 Test frameworks (defaults)

Widely recognized defaults:
- GoogleTest for unit/integration tests.
- Catch2 is acceptable via DEC.

Framework choice MUST comply with dependency policy.

---

## 8. Exported targets and install rules (conditional)

If the project builds libraries intended for consumption:
- Define install rules:
  - `install(TARGETS ...)`
  - `install(DIRECTORY include/... )`
- Provide CMake package config exports:
  - `install(EXPORT ...)` and config files under `lib/cmake/<project>`

If the project is not intended for installation, document via DEC.

---

## 9. CMakePresets.json guidance (normative)

### 9.1 Required preset types

Presets MUST include:
- configure presets per OS
- build presets per OS
- test presets per OS

Example naming:
- configure: `linux`, `windows`, `macos`
- build: `linux`, `windows`, `macos`
- test: `linux`, `windows`, `macos`
- CI aggregates: `ci-linux`, `ci-windows`, `ci-macos`

Naming may vary, but deviations require DEC and validator awareness.

### 9.2 Generator defaults

- Linux/macOS: Ninja recommended
- Windows: Ninja or Visual Studio generator; MUST be pinned in preset
- Use `binaryDir` to isolate build outputs per preset.

### 9.3 Cache variables

Presets SHOULD set:
- `CMAKE_BUILD_TYPE` (for single-config generators)
- options: `<PROJ>_BUILD_TESTS`, `<PROJ>_ENABLE_WARNINGS_AS_ERRORS`, etc.

For multi-config generators (Visual Studio):
- specify `configuration` in build/test presets.

---

## 10. Reproducibility and environment reporting (recommended)

CI SHOULD:
- print compiler version
- print CMake version
- print generator
- print host OS and architecture
- archive `compile_commands.json` for debugging (optional)

---

## 11. Required build commands (Ch7 linkage)

Ch7 MUST document exact commands that CI runs, typically:
- `cmake --preset ci-linux`
- `cmake --build --preset ci-linux`
- `ctest --preset ci-linux --output-on-failure`

Similarly for windows/macos.

---

## 12. Common anti-patterns (forbidden or discouraged)

Forbidden (or strongly discouraged without DEC):
- hard-coded absolute paths
- downloading dependencies during configure/build (must be controlled by dependency policy)
- non-deterministic code generation without pinned inputs
- custom ad-hoc scripts replacing presets in CI

---

## 13. Minimal example (informative)

Root CMakeLists essentials:

```cmake
cmake_minimum_required(VERSION 3.28)
project(myproj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MYPROJ_BUILD_TESTS "Build tests" ON)
option(MYPROJ_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

add_library(lib_core)
target_sources(lib_core PRIVATE src/core.cpp)
target_include_directories(lib_core PUBLIC include)
```

---

## 14. Policy changes

Changes to this playbook MUST be introduced via CR and MUST include:
- validator updates if presets schema changes,
- migration notes for existing projects.
