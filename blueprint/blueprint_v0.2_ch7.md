# Blueprint v0.2 — Chapter 7: Build, Toolchain, CI/CD

## 7.1 Supported Toolchains
inherit from /blueprint/blueprint_v0.1_ch7.md

## 7.2 CMake Structure & Targets
inherit from /blueprint/blueprint_v0.1_ch7.md

### 7.2.1 v0.2 Targets (IDE additions)
- [BUILD-07] New/expanded targets for “Full IDE” work:
  - `arcanee_app` (existing): now boots into dockspace IDE ([REQ-63]).
  - `arcanee_ide` (new internal library): IDE subsystems (UIShell/Project/Doc/TextBuffer/Parse/Search/Tasks/Timeline/LSP/DAP).
  - `arcanee_ide_tests` (new): unit + integration tests for IDE ([TEST-27..38]).
  - `arcanee_squirrel_lsp` (new tool): Squirrel-only LSP server process per [DEC-42] (used by [TEST-34]).
  - `arcanee_assets_verify` (new): validates required IDE asset files exist in build/package tree (tree-sitter query files) ([REQ-69], [DEC-36]).

> Naming is illustrative; keep repo naming conventions. If the repo already has a `tests` target pattern, mirror it.

## 7.3 Build Options / Feature Flags (v0.2)
- [BUILD-08] Add the following options (default values reflect v0.2 DoD):
  - `ARCANEE_ENABLE_IDE=ON` (master toggle; must be ON for v0.2)
  - `ARCANEE_ENABLE_LSP=ON` (Squirrel-only LSP; can be OFF for minimal builds; IDE still works)
  - `ARCANEE_ENABLE_DAP=ON` (DAP client + baseline ARCANEE adapter)
  - `ARCANEE_ENABLE_TIMELINE=ON`
  - `ARCANEE_ENABLE_TASKS=ON`
- [DEC-52] **Feature flag policy**
  - Decision: feature flags exist only to allow developer bring-up and platform triage; v0.2 release presets must enable all DoD IDE features.
  - Rationale: keep CI deterministic while allowing partial builds during development.
  - Alternatives: no flags; or compile-time plugins.
  - Consequences: CI presets assert expected default values (see 7.6).

## 7.4 Dependencies & Pinning
inherit from /blueprint/blueprint_v0.1_ch7.md

### 7.4.1 v0.2 dependency additions
- [BUILD-07] Add & pin (via FetchContent pins consistent with current repo policy):
  - tree-sitter (core)
  - tree-sitter-squirrel (grammar)
  - PCRE2
  - SQLite
  - zstd
  - xxHash
  - tomlplusplus
  - nlohmann/json
  - spdlog (if not already pinned)
- [DEC-27] ImGui docking branch is consumed via **DiligentTools ImGui integration** (source of truth); do not add a standalone ImGui submodule.

### 7.4.2 tree-sitter assets
- [REQ-69] Ship query files in repo under:
  - `assets/ide/treesitter/squirrel/queries/highlights.scm`
  - `assets/ide/treesitter/squirrel/queries/locals.scm`
  - `assets/ide/treesitter/squirrel/queries/injections.scm`
- [BUILD-09] CMake must install/copy these assets into the runtime asset directory used by the app, and `arcanee_assets_verify` must fail the build if any are missing.

### 7.4.3 PCRE2 configuration
- [DEC-53] **PCRE2 JIT**
  - Decision: build PCRE2 with JIT enabled where available, but do not hard-require JIT at runtime (feature-detect and fall back).
  - Rationale: JIT improves workspace search performance, but availability differs by platform and build config.
  - Alternatives: always-on JIT (hard fail if missing) or no JIT.
  - Consequences: [TEST-29] must pass with JIT disabled; perf harness may log whether JIT is active.

## 7.5 Build Flags & Warnings
inherit from /blueprint/blueprint_v0.1_ch7.md

### 7.5.1 v0.2 IDE compile-time rules
- [BUILD-10] Enforce UTF-8 text baseline in IDE:
  - normalize loaded files to UTF-8; if invalid UTF-8 encountered, return a recoverable `Status` and show error (do not crash).
- [BUILD-11] Enforce `[TEMP-DBG]` policy: CI fails if any `[TEMP-DBG]` is present (see 7.6).

## 7.6 CI Gates (Minimum Required)
inherit from /blueprint/blueprint_v0.1_ch7.md

### 7.6.1 v0.2 additional required gates
- [TEST-01] `check_no_temp_dbg` must run on every CI build.
- [TEST-02] `check_no_na_without_dec` must run on every CI build.
- [TEST-03] `check_req_has_test_and_checklist` must run on every CI build and include:
  - new requirements [REQ-63..86] plus [REQ-77..78], [REQ-80..85]
  - new tests [TEST-27..38]
- [TEST-04] `check_inherit_targets` must run on every CI build.

### 7.6.2 v0.2 test matrix (required on PR)
- [BUILD-12] Required PR jobs:
  1) Configure + build (Release)
  2) Unit/integration tests:
     - [TEST-27] Dockspace/layout persistence
     - [TEST-28] TextBuffer property tests
     - [TEST-29] SearchService tests
     - [TEST-30] ParseService tests
     - [TEST-31] TimelineStore tests
     - [TEST-32] TaskRunner tests
     - [TEST-33] DAP tests (baseline ARCANEE adapter)
     - [TEST-34] Settings/keybindings + LSP smoke
  3) Performance micro-bench (fast):
     - [TEST-35] typing micro-bench (enforces [DEC-34])
     - [TEST-36] snapshot micro-bench (enforces [DEC-34])

### 7.6.3 Nightly / extended CI
- [DEC-54] **Nightly stress tests**
  - Decision: run the following in nightly CI (not required on PR due to runtime variance), but they remain mandatory for release readiness:
    - [TEST-37] Search/edit cancel stress
    - [TEST-38] Timeline snapshot stress
  - Rationale: these are long-running and sensitive to CI noise; nightly provides better signal.
  - Alternatives: run on every PR; or never run in CI.
  - Consequences:
    - Release checklist must include “last green nightly” requirement referencing [TEST-37], [TEST-38].
    - Failures must block release tagging (Ch9 release policy).

## 7.7 Reproducible Builds & Asset Packaging
inherit from /blueprint/blueprint_v0.1_ch7.md

### 7.7.1 v0.2 IDE asset packaging rules
- [BUILD-09] `arcanee_assets_verify` checks:
  - tree-sitter query files exist and are readable
  - (optional) queries compile/load without errors in a headless test harness
- [REQ-85] Default ignore patterns must ensure `.arcanee/timeline/` and rebuildable artifacts are not included in cartridge exports or distributable packages.

## 7.8 Developer Commands (runnable)
inherit from /blueprint/blueprint_v0.1_ch7.md

### 7.8.1 v0.2 additions (example presets/commands)
> Keep existing presets; add/extend as needed. The commands below are normative examples for the checklist/tests.

- [BUILD-13] Configure + build (Release):
  - `cmake --preset <release-preset>`
  - `cmake --build --preset <release-build-preset>`
- [BUILD-14] Run IDE tests:
  - `ctest --preset <test-preset> --output-on-failure`
- [BUILD-15] Run perf micro-bench gates:
  - `ctest --preset <test-preset> -R "(TEST-35|TEST-36)" --output-on-failure`
- [BUILD-16] Run asset verification:
  - `cmake --build --preset <release-build-preset> --target arcanee_assets_verify`

## 7.9 Tooling Integration (LSP server tool)
- [DEC-42] LSP server is shipped in-repo as `arcanee_squirrel_lsp`.
- [BUILD-17] CI must build `arcanee_squirrel_lsp` whenever `ARCANEE_ENABLE_LSP=ON`.
- [TEST-34] LSP smoke must launch the tool and validate at least one diagnostics event path for a known-bad Squirrel sample.


