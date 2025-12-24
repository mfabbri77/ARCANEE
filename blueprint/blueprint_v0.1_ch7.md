# Blueprint v0.1 — Chapter 7: Build System, Toolchain, CI

## 7.1 Toolchain Baselines (v1.0)
- [REQ-52] Minimum toolchains:
  - Linux: GCC 11 or Clang 14
  - Windows: MSVC 2019+ or Clang 14+
  - macOS: Xcode/Clang 14+
- [REQ-18] C++17 required.

## 7.2 CMake Structure
- [REQ-53] Provide CMakePresets.json with at least:
  - `dev` (Debug)
  - `rel` (Release)
  - `asan` (Linux)
  - `tsan` (Linux best-effort)
- [REQ-54] Provide CTest presets matching build presets.

## 7.3 Dependencies (FetchContent)
- [DEC-15] Keep FetchContent for v1.0.
- [REQ-55] All FetchContent deps must be pinned (tag/commit) and recorded in a generated dependency manifest.
- [REQ-56] Builds must be reproducible under CI caching (no unpinned “main/master” fetches).

## 7.4 Feature Flags
- [REQ-57] CMake options (defaults):
  - `ARCANEE_ENABLE_OPENGL_FALLBACK=OFF` (Linux optional)
  - `ARCANEE_ENABLE_LIBOPENMPT=ON` (auto-detect; OFF allowed)
  - `ARCANEE_ENABLE_WORKBENCH=ON` (v1.0 required)
  - `ARCANEE_HEADLESS=OFF` (test/smoke modes)

## 7.5 CI Gates (must pass for v1.0)
- [TEST-01] No `[TEMP-DBG]`.
- [TEST-02] No `N/A` without `[DEC-XX]`.
- [TEST-03] Every `[REQ-*]` mapped to ≥1 `[TEST-*]` and checklist step.
- [TEST-19] `format_check`: clang-format (or equivalent) check.
- [TEST-20] `build_all`: build Debug+Release per platform.
- [TEST-21] `ctest_all`: run tests (unit + smoke).
- [TEST-22] `package_portable`: produce portable folder layout artifact.

## 7.6 Suggested Commands (runnable)
- Configure/build:
  - `cmake --preset dev`
  - `cmake --build --preset dev`
- Tests:
  - `ctest --preset dev`
- Release:
  - `cmake --preset rel`
  - `cmake --build --preset rel`
  - `ctest --preset rel`
- Portable packaging (target to implement):
  - `cmake --build --preset rel --target package_portable`

## 7.7 “No Networking” Hard Gate
- [REQ-13] Networking is out of scope; enforce by:
  - banning networking libs in link line (CI grep)
  - no socket usage in code (static analysis allowlist)
- [TEST-23] `check_no_networking_symbols` gate.


