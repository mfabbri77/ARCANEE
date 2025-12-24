# CI Reference (Normative)
This document defines a **golden CI baseline** for native C++ repositories (and optional Python bindings) with strong quality gates: formatting, lint, sanitizers, tests, benchmarks (optional), and **TEMP-DBG** enforcement.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) CI Goals
- Ensure every change is buildable and testable on the supported platform matrix.
- Enforce repository hygiene (format, lint, no TEMP-DBG).
- Catch memory/thread/UB issues early (sanitizers).
- Prevent performance regressions (optional but recommended for perf-critical projects).
- Produce deterministic artifacts for releases.

---

## 2) Mandatory Gates (Must-Fail Conditions)
The CI must fail the change if any of the following fail:
1) **Build succeeds** (per platform matrix)
2) **Unit/Integration tests pass** (`ctest ...`)
3) **Sanitizers pass** (at least ASan+UBSan on Linux; TSan where feasible)
4) **Formatting is clean** (`format_check`)
5) **No TEMP-DBG markers** (`check_no_temp_dbg`)
6) **Packaging/install/export checks** (if library/SDK ships artifacts)

Optional but recommended:
- clang-tidy (lint)
- fuzz smoke tests
- benchmark regression thresholds

---

## 3) Platform Matrix (Recommended)
Define in the Blueprint ([ARCH-XX]) and implement in CI:
- **Linux (primary):** clang (preferred) + gcc (optional), Ninja, ASan/UBSan/TSan
- **Windows:** MSVC or clang-cl, Ninja, unit tests, (sanitizers limited)
- **macOS:** AppleClang, Ninja, unit tests (sanitizers where feasible)

Minimum recommended:
- Linux + one of (Windows or macOS).
If the project targets all three, CI must run all three.

---

## 4) Dependency Caching (Mandatory)
CI must cache dependency builds:
- **vcpkg**: cache `VCPKG_DEFAULT_BINARY_CACHE` (or GH Actions cache path) + lockfile/manifest keying
- **conan**: cache conan home + lockfiles

Rules:
- Cache key must include OS + compiler + dependency manifest hash.
- Cache should be read-only on PR forks if security policy requires it.

---

## 5) Standard Job Set (Golden Pipeline)
### 5.1 Preflight (fast)
- `check_no_temp_dbg`
- `format_check`
- `configure` + minimal build (dev or ci preset)

### 5.2 Build & Test (per platform)
- Configure with preset (e.g., `ci`)
- Build
- `ctest --output-on-failure` (preset-backed)

### 5.3 Sanitizers (Linux focused)
Run at least:
- `asan` preset
- `ubsan` preset  
And, where supported:
- `tsan` preset

Rules:
- Sanitizer jobs must run tests (not only build).
- If a sanitizer is unsupported on a platform, document it under [BUILD-04]/[TEST-XX].

### 5.4 Lint (recommended)
- `lint` target (clang-tidy) or equivalent
- May be allowed to be “non-blocking” initially, but goal is to make it blocking.

### 5.5 Bench (optional, for perf budgets)
- Run microbenchmarks on a stable runner type if possible.
- Enforce regression thresholds (see `performance_benchmark_harness.md` if present).

### 5.6 Packaging (if shipping)
- `cmake --build --preset release`
- `cmake --install ...` to a staging directory
- Verify exported config (`<proj>Config.cmake`) loads in a minimal downstream consumer test.

---

## 6) Required Commands (CMake Presets Convention)
CI should use presets only (avoid ad-hoc flags).
Typical commands:
- `cmake --preset <preset>`
- `cmake --build --preset <preset>`
- `ctest --preset <preset> --output-on-failure`
Quality gates:
- `cmake --build --preset <preset> --target check_no_temp_dbg`
- `cmake --build --preset <preset> --target format_check`
- `cmake --build --preset <preset> --target lint` (if enabled)

---

## 7) TEMP-DBG Gate (Mandatory)
CI must run the `check_no_temp_dbg` target (or script) on every PR/merge.
- Fail if any `[TEMP-DBG] START` or `[TEMP-DBG] END` is present in tracked sources.
- Print file + line numbers.

Reference: `temp_dbg_policy.md`.

---

## 8) Artifacts & Logs (Recommended)
Always upload on failure:
- `CTest` logs
- sanitizer reports (if generated)
- `compile_commands.json` (optional; helpful for debugging)

On release builds:
- built packages/archives
- SBOM (if applicable)
- generated docs (optional)

---

## 9) Release Workflow (Recommended)
### 9.1 Tag-driven releases
- Tags: `v<MAJOR>.<MINOR>.<PATCH>`
- On tag:
  - run full matrix + sanitizers (where applicable)
  - build release artifacts
  - publish artifacts (registry/release page)

### 9.2 Required release files
Enforce presence of:
- `CHANGELOG.md`
- `MIGRATION.md` if MAJOR
- updated `/blueprint` snapshot for that release

---

## 10) Security Notes (Practical)
- Pin dependencies (manifest/lockfiles).
- Avoid executing untrusted code from PR forks in privileged contexts.
- If using caches, ensure keys are not attacker-controlled.

---

## 11) Minimal GitHub Actions Skeleton (Reference)
This is a conceptual reference only; projects may adapt while preserving gates.

```yaml
name: ci
on: [pull_request, push]
jobs:
  linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake --preset ci
      - name: Gates
        run: |
          cmake --build --preset ci --target check_no_temp_dbg
          cmake --build --preset ci --target format_check
      - name: Build
        run: cmake --build --preset ci
      - name: Test
        run: ctest --preset ci --output-on-failure
  asan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: cmake --preset asan
      - run: cmake --build --preset asan
      - run: ctest --preset asan --output-on-failure
```

---

## 12) Compliance Checklist
- [ ] CI runs on declared platform matrix ([ARCH-XX])
- [ ] `check_no_temp_dbg` and `format_check` are blocking gates
- [ ] Tests run on every PR/merge
- [ ] ASan+UBSan run at least on Linux
- [ ] Dependency caching is configured and keyed safely
- [ ] Release workflow produces deterministic artifacts and enforces required docs
