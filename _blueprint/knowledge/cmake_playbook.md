<!-- _blueprint/knowledge/cmake_playbook.md -->

# CMake Playbook (Normative)
This document defines a consistent, production-grade **CMake strategy** for native C++ apps/libraries.  
It is intended to be used by Blueprints and AI coding agents to produce repositories that build cleanly across platforms with predictable options and quality gates.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this playbook → project-specific constraints.

---

## 1) Core Principles
1) **Targets-first:** everything is a target; no global include dirs/flags unless justified.  
2) **Options are explicit:** every toggle is an option with a default and documentation.  
3) **Presets are canonical:** developers use `CMakePresets.json`; CI uses presets too.  
4) **Exportable:** libraries support `install()` + `export()` (unless explicitly N/A).  
5) **Quality gates are enforceable:** format/lint/sanitizers/tests and `check_no_temp_dbg` are build targets.

---

## 2) Repository Layout (Recommended)
```
/CMakeLists.txt
/CMakePresets.json
/cmake/                 # helper modules, toolchains, Find*.cmake if needed
/include/<proj>/        # public headers
/src/                   # private implementation
/tests/                 # unit/integration tests
/examples/              # optional
/tools/                 # scripts: check_no_temp_dbg, tooling helpers
/docs/                  # docs, changelog, migration
/blueprint/             # specs (source-of-truth)
```

---

## 3) Targets & Naming (Normative)
Use consistent names:
- `<proj>`: main library target (static or shared)
- `<proj>_obj`: optional object library for sharing compilation units
- `<proj>_tests`: test binary (or multiple targets)
- `<proj>_bench`: benchmark binary (optional)
- `format`, `format_check`: formatting targets
- `lint`: clang-tidy/cppcheck (optional but recommended)
- `check_no_temp_dbg`: required gate target

**Rule:** Public headers belong to `<proj>` and are installed/exported (unless app-only).

---

## 4) Build Types & Presets (Mandatory)
### 4.1 Preset set (recommended minimum)
- `dev` (Debug-like, fast iteration)
- `release` (optimized shipping config)
- `asan` (AddressSanitizer)
- `ubsan` (UndefinedBehaviorSanitizer)
- `tsan` (ThreadSanitizer, where supported)
- `ci` (CI deterministic build, often Release + warnings-as-errors)

### 4.2 Generator policy
- Prefer **Ninja** for single-config builds (Linux/macOS/Windows).
- For MSVC, Ninja + clang-cl or MSVC is acceptable; multi-config generators are allowed but must be codified.

### 4.3 Toolchain policy
Blueprint must specify:
- compilers (clang/gcc/msvc)
- standard library (libstdc++/libc++)
- C++ standard (17/20/23)
- Windows CRT policy (MD/MT)

---

## 5) Options (Standard Set)
Provide these options (with documented defaults):
- `PROJ_BUILD_TESTS` (ON in dev/ci, OFF in release packaging optional)
- `PROJ_BUILD_EXAMPLES` (OFF by default)
- `PROJ_BUILD_BENCH` (OFF by default)
- `PROJ_ENABLE_WERROR` (ON in ci, OFF in dev by default)
- `PROJ_ENABLE_LTO` (ON in release if supported)
- `PROJ_ENABLE_SANITIZERS` (preset-controlled; avoid mixing)
- `PROJ_ENABLE_PCH` (optional; default OFF unless proven helpful)
- `PROJ_ENABLE_UNITY` (optional; default OFF)
- `PROJ_INSTALL` (ON for libraries; OFF for app-only repos if desired)

**Rule:** Presets should set these options; developers should rarely pass ad-hoc `-D` flags.

---

## 6) Compiler/Linker Flags (Guidelines)
### 6.1 Warnings baseline
- Enable a strong baseline for Clang/GCC (e.g., `-Wall -Wextra -Wpedantic`)
- Add additional warnings carefully to avoid noise
- MSVC: `/W4` (or `/W3` if ecosystem requires) and `/permissive-` where feasible

### 6.2 Werror policy
- Enforce warnings-as-errors in CI/release gating builds.
- Allow local dev to disable werror for velocity.

### 6.3 Optimization & debug info
- `dev`: debug symbols ON; optimization modest/off as needed
- `release`: `-O3` or equivalent; symbols policy defined (stripped vs separate debug symbols)

### 6.4 LTO/IPO
- Prefer LTO in `release` if it does not break toolchains.
- If enabled, document link-time cost and CI implications.

### 6.5 Visibility
For libraries, prefer:
- GCC/Clang: hidden visibility by default (documented in [API-XX]/[BUILD-XX])
- Windows: explicit export macro usage

---

## 7) Sanitizers (Mandatory presets)
### 7.1 Preset separation
Do not combine sanitizers unless you explicitly know they are compatible.
- `asan` + `ubsan` may be combined on some platforms; codify only if tested.
- `tsan` is often incompatible with `asan`.

### 7.2 Runtime config
- Set `ASAN_OPTIONS`, `TSAN_OPTIONS`, `UBSAN_OPTIONS` in CI where needed.
- Ensure the Blueprint defines “how to run” and expected failure modes.

---

## 8) Dependencies (vcpkg/conan) Integration
Blueprint chooses **one** primary dependency manager (recommended):
- vcpkg manifest mode (`vcpkg.json`) OR conan (`conanfile.py`/`conanfile.txt`)

**Rules:**
- Dependencies must be **pinned** (versions/overrides).
- CI must cache dependency builds.
- Exported package config must not leak private deps into public usage requirements.

---

## 9) Install/Export (Libraries)
If producing a library:
- Provide `install(TARGETS ...)` and `install(EXPORT ...)`
- Install headers to `include/<proj>/`
- Generate package config:
  - `<proj>Config.cmake`, `<proj>Targets.cmake`
- Optionally generate `<proj>ConfigVersion.cmake` for SemVer checks.

**Rule:** Public vs private include dirs must be correct:
- `target_include_directories(<proj> PUBLIC $<BUILD_INTERFACE:...> $<INSTALL_INTERFACE:...>)`

---

## 10) Tooling Targets (Quality Gates)
### 10.1 Formatting
- Provide `format` (applies clang-format) and `format_check` (fails if changes needed)
- Codify the clang-format version expectation (or vendor a config file)

### 10.2 Lint
- `lint` target (clang-tidy) recommended
- Avoid making lint “always on” in dev unless the project demands it; enforce in CI gate if feasible

### 10.3 TEMP-DBG gate (Mandatory)
- Implement `check_no_temp_dbg` target that runs `/tools/check_no_temp_dbg.*`
- CI must run `check_no_temp_dbg` and fail on markers.

### 10.4 Compile commands
- Always enable `CMAKE_EXPORT_COMPILE_COMMANDS` in dev/ci presets for tooling.

---

## 11) Testing Integration
- Use `CTest` (`enable_testing()` + `add_test(...)`)
- Tests must run under sanitizer presets
- Prefer fast unit tests in CI; heavier soak/stress can be separate jobs

**Rule:** `ctest --output-on-failure` should be used in CI.

---

## 12) Packaging Notes (Optional)
If shipping artifacts:
- Define `install` + `cpack` policy only if needed
- Keep packaging deterministic; do not embed machine-specific absolute paths
- Version stamps should be controlled (e.g., from git tag, with a fallback)

---

## 13) Minimal Presets Skeleton (Reference)
Below is a conceptual reference. Projects may adjust but must keep the preset names stable.

```json
{
  "version": 6,
  "configurePresets": [
    { "name": "dev", "generator": "Ninja", "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "PROJ_BUILD_TESTS": "ON",
        "PROJ_ENABLE_WERROR": "OFF"
    }},
    { "name": "release", "generator": "Ninja", "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "PROJ_BUILD_TESTS": "OFF",
        "PROJ_ENABLE_LTO": "ON"
    }}
  ],
  "buildPresets": [
    { "name": "dev", "configurePreset": "dev" },
    { "name": "release", "configurePreset": "release" }
  ],
  "testPresets": [
    { "name": "dev", "configurePreset": "dev", "output": {"outputOnFailure": true} }
  ]
}
```

---

## 14) Compliance Checklist
- [ ] Presets exist: dev/release/asan/ubsan/tsan (as applicable)
- [ ] Options standardized and set by presets
- [ ] Library repos implement install/export correctly
- [ ] `check_no_temp_dbg` target exists and is used by CI
- [ ] `format_check` and tests are CI gates
- [ ] Sanitizer presets are runnable and documented
