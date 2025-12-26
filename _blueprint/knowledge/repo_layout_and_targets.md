<!-- repo_layout_and_targets.md -->

# Repository Layout and Targets (Normative)

This document defines the **canonical repository structure**, required build targets, and naming conventions for projects governed by the DET CDD blueprint system.

It is **normative** unless a higher-precedence rule overrides it via **DEC + enforcement gates**.

---

## 1. Goals

- Ensure a predictable repo structure across platforms.
- Ensure CI can build and test deterministically using standardized targets.
- Provide a stable mapping between blueprint components and build artifacts.

---

## 2. Canonical repository layout

### 2.1 Top-level directories (required)

The repository root MUST contain:

- `/_blueprint/` — Source of Truth for architecture and release overlays
- `/src/` — production source
- `/include/` — public headers (or at minimum, headers used externally)
- `/tests/` — tests (unit/integration/other)
- `/docs/` — user-facing docs (not blueprint)
- `/tools/` — scripts (validator, generators, CI helpers)
- `/examples/` — sample apps or usage snippets
- `/CMakeLists.txt` — root build entrypoint
- `/CMakePresets.json` — required (see XPLAT rules)

Allowed additional top-level directories (common):
- `/third_party/` (if vendoring is allowed by dependency policy)
- `/cmake/` (CMake modules)
- `/.github/` (CI workflows) or equivalent CI configuration directory
- `/.clang-format`, `/.clang-tidy`, `/.editorconfig`

### 2.2 Blueprint directory structure (required)

`/_blueprint/` MUST contain:

- `/_blueprint/_knowledge/` — reference knowledge files (read-only at runtime)
- `/_blueprint/rules/` — enforceable rule files used by validator/CI
- `/_blueprint/project/` — baseline blueprint:
  - `ch0_meta.md` … `ch9_versioning_lifecycle.md`
  - `decision_log.md`
  - `walkthrough.md`
  - `implementation_checklist.yaml`
- `/_blueprint/vM.m/` — one or more overlays:
  - `release.md`
  - `decision_delta_log.md`
  - `walkthrough_delta.md`
  - `checklist_delta.yaml`
  - `CR-XXXX.md` files
  - changed chapter files as needed

---

## 3. Naming conventions (normative)

### 3.1 C++ targets

Target names MUST be lowercase with underscores, using these prefixes:

- Libraries:
  - `lib_<name>` (static or shared, specified explicitly by options)
- Executables:
  - `app_<name>` (production)
  - `tool_<name>` (developer/CI tooling)
  - `example_<name>` (examples)
- Tests:
  - `test_<name>` (test executables)

### 3.2 CMake options

CMake cache options MUST use:

- `PROJECTNAME_<FEATURE>` (upper snake)
- boolean values: `ON/OFF`

Common required options (defaults defined in `cmake_playbook.md`):
- `PROJECTNAME_BUILD_TESTS`
- `PROJECTNAME_BUILD_EXAMPLES`
- `PROJECTNAME_ENABLE_SANITIZERS`
- `PROJECTNAME_ENABLE_LTO` (if supported)
- `PROJECTNAME_ENABLE_WARNINGS_AS_ERRORS` (CI default ON)

### 3.3 Include paths and header naming

- Public headers MUST live under `/include/<project_or_namespace>/...`.
- Public headers SHOULD be stable and minimal.
- Private/internal headers SHOULD live under `/src/` or `/include/<project>/detail/` if they must be installed (discouraged; requires DEC).

---

## 4. Required build outputs

### 4.1 Core targets (required)

Every conforming repo MUST provide at minimum:

- `lib_core` — primary library containing core functionality **or** a project-specific core library (DEC if named otherwise)
- `tool_blueprint_validator` — a target or script entrypoint enabling CI to validate blueprint compliance
- `test_core` — at least one test target that runs unit tests

If the project is application-only (no library), a DEC MUST document:
- why no library exists,
- what the “core artifact” is instead,
- and equivalent targets to support testing and packaging.

### 4.2 “All tests” aggregator target (required)

Repo MUST provide at least one of:
- CTest integration (`ctest`) via `enable_testing()` and `add_test()`
- OR an explicit target `test_all` that runs all tests

Preferred: CTest with `ctest --test-dir <build> --output-on-failure`.

### 4.3 Format/lint targets (recommended)

Provide targets or scripts (preferred as CMake custom targets) for:

- `tool_format` — clang-format applied to repo
- `tool_lint` — clang-tidy (or equivalent)
- `tool_static_analysis` — optional aggregate (MSVC /analyze, scan-build, etc.)

CI SHOULD run `tool_format --check` style equivalents.

---

## 5. Required scripts and tools

### 5.1 Blueprint composer/validator (COMP-01)

Repo MUST include:

- A script under `/tools/blueprint/` (recommended):
  - `compose_validate.py` (or equivalent)
- Or a compiled tool target:
  - `tool_blueprint_validator`

This tool MUST:
- reconstruct Eff(vCURRENT) and validate schema and gates (per `blueprint_schema.md`)
- provide deterministic exit codes and actionable error messages
- support local invocation consistent with CI commands

### 5.2 Standard tool directories

Tools SHOULD be organized as:
- `/tools/blueprint/` — composer/validator, ID scanners, schema checks
- `/tools/ci/` — CI helpers (cache keys, env reports)
- `/tools/dev/` — developer workflow scripts

---

## 6. Test layout (normative)

- Tests MUST live under `/tests/`.
- Unit tests SHOULD mirror library/module structure:
  - `/tests/<module>/test_<module>_*.cpp`
- Integration tests MAY live under:
  - `/tests/integration/`
- Fuzz tests MAY live under:
  - `/tests/fuzz/` (gated; may require extra CI runners)
- Performance benchmarks SHOULD live under:
  - `/tests/perf/` or `/benchmarks/` (DEC required if separate top-level dir)

---

## 7. Packaging layout (normative default)

Default packaging assumes:
- installable headers under `/include/`
- installable libraries under a standard install prefix
- CMake package config files under `lib/cmake/<project>`

If “internal-only packaging” is used:
- it MUST be introduced via DEC + gate,
- and must not leak as public distribution artifacts.

---

## 8. Documentation layout

- `/docs/` is for user/developer documentation (non-blueprint).
- The blueprint (`/_blueprint/`) is architecture SoT and MUST NOT be duplicated in `/docs/`.

---

## 9. Validator checks (recommended)

Validator/CI SHOULD check:
- required directories exist (or DEC exists explaining deviations)
- required targets exist (or DEC exists for exceptions)
- CMakePresets.json exists and contains required presets
- `tool_blueprint_validator` or script is runnable
- tests are discoverable by ctest or `test_all`
- header/include layout follows policy

---

## 10. Widely recognized defaults (recommended)

- Prefer:
  - `src/` for implementation `.cpp`
  - `include/` for public headers
  - `tests/` with GoogleTest or Catch2
  - CMake + Ninja + presets
  - GitHub Actions-like CI with ubuntu/windows/macos matrix

Any deviation MUST be captured by DEC with enforcement (tests/gates).

---

## 11. Minimal example tree (informative)

```text
/
  CMakeLists.txt
  CMakePresets.json
  include/
    myproj/
      core.hpp
  src/
    core.cpp
  tests/
    core/
      test_core.cpp
  tools/
    blueprint/
      compose_validate.py
  _blueprint/
    _knowledge/
    rules/
    project/
      ch0_meta.md
      ...
    v1.0/
      release.md
      ...
```
