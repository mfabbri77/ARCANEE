# Blueprint v0.3 — Chapter 7: Build, Toolchain, CI

## Build/Toolchain Baseline
inherit from blueprint_v0.2_ch7.md

---

## [BUILD-18] New Dependencies (v0.3)
### Tree-sitter TOML grammar
- Add `tree-sitter-toml` grammar as a pinned dependency (commit SHA or immutable tag).
- Build as a static library from the grammar C sources (typically `src/parser.c` and, if present, `src/scanner.c`).
- Expose a C symbol `tree_sitter_toml()` and link it into the IDE binary.

### System font backends
- Windows: link DirectWrite (`dwrite`) (and any minimal required Win32 libs).
- macOS: link CoreText/CoreFoundation.
- Linux: prefer `fontconfig` (FindPkgConfig + pkg-config), but treat as **optional**:
  - If present: enable FontLocatorFontconfig
  - If absent: build still succeeds with a fallback font strategy, and runtime emits a warning diagnostic when a non-default family is requested. [DEC-65]

### TOML parsing
- Continue using `toml++` (already present) for config parsing and DOM. [DEC-64]

---

## [BUILD-19] Repo-root `config/` Packaging Artifact
- `config/color-schemes.toml` must exist in source control and in packaged artifacts. [REQ-90]
- `config/editor.toml`, `config/gui.toml`, `config/keys.toml` may be absent (optional; defaults apply). [REQ-90]
- CI must fail if `config/color-schemes.toml` is missing from the repo for v0.3+. [REQ-90]

---

## [BUILD-20] CMake Integration (v0.3 delta)
### Targets
- Add a target (or sources) for `tree-sitter-toml` and link it to the IDE executable target.
- Ensure the `assets/ide/treesitter/toml/queries/highlights.scm` file is installed/copied alongside existing assets. [REQ-94]

### Options
- Add an option to control fontconfig usage on Linux:
  - `ARCANEE_USE_FONTCONFIG` (default ON if found)
  - If OFF or not found, compile fallback backend.

### Include discipline
- New internal headers for config/theme/keymap/font locator remain private to `/src` (app-internal), consistent with app-only repo layout. [DEC-68]

---

## [BUILD-21] CI Gates (v0.3 delta)
CI must additionally enforce:
- **No `[TEMP-DBG]` remains** (existing gate).  
- **No `N/A` without `[DEC-XX]`** (existing gate).  
- **Traceability gate**: each of [REQ-90..98] is referenced by:
  - ≥1 [TEST] entry, and
  - ≥1 checklist step (in `/blueprint/implementation_checklist.yaml`).  
- **Assets gate**:
  - `assets/ide/treesitter/toml/queries/highlights.scm` exists. [REQ-94]
  - `config/color-schemes.toml` exists. [REQ-90]
- **Dependency pinning gate** (if already present in v0.2 CI policy): new deps for tree-sitter TOML must be pinned to immutable refs (commit SHA preferred). [BUILD-18]

---

## [BUILD-22] Test Execution (v0.3 delta)
Add to CI test suite:
- [TEST-39] Config parse/validate/fallback + diagnostics (unit/integration)
- [TEST-40] Unified theme application (golden test for derived ImGui mapping + token palette)
- [TEST-41] Keybinding determinism and `primary` normalization
- [TEST-42] Fonts system-only enforcement + last-known-good retention (mock backend for CI; platform backend smoke where feasible)
- [TEST-43] Configuration menu + config explorer root smoke
- [TEST-45] Hot-apply thread safety + `reload_seq` determinism (TSan where available)

---

## [BUILD-23] Runnable Commands (v0.3)
inherit from blueprint_v0.2_ch7.md

Add (examples; exact preset names inherit from v0.2):
- Build:
  - `cmake --preset <dev>`  
  - `cmake --build --preset <dev>`
- Tests:
  - `ctest --preset <dev> --output-on-failure`
- Sanitizers (where configured in v0.2):
  - `cmake --preset <tsan>` → `ctest --preset <tsan> --output-on-failure`

---

## [DEC-72] Linux Fontconfig Optionality
- On Linux, the build must succeed without fontconfig installed; runtime falls back to a deterministic default font and emits diagnostics when exact family selection cannot be honored. [DEC-65], [REQ-92]

