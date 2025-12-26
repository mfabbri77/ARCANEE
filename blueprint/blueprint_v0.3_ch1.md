# Blueprint v0.3 — Chapter 1: Scope, Requirements, Budgets

## [SCOPE-01] Scope Tags
inherit from blueprint_v0.2_ch1.md

## [SCOPE-02] In-Scope (v0.3 delta)
This release adds a repo-root TOML configuration system with in-IDE editing, hot-apply, unified theming, Tree-sitter TOML highlighting, and TOML keybindings. [CR-0044]

- `./config/` directory (repo root) with:
  - `config/color-schemes.toml` (shipped by default; user-editable)
  - `config/editor.toml` (optional)
  - `config/gui.toml` (optional)
  - `config/keys.toml` (optional)  [REQ-90], [DEC-67]
- Hot-apply on **IDE save** of `config/*.toml` (debounced), non-fatal validation with per-field fallback + Problems diagnostics. [REQ-91], [REQ-97]
- Unified color scheme (single source of truth) driving:
  - editor UI colors
  - Tree-sitter token colors
  - ImGui Dock style colors (derived mapping MVP; optional explicit overrides). [REQ-93], [DEC-63]
- Tree-sitter highlighting support for:
  - `.nut` (Squirrel)
  - `.toml` (TOML grammar + highlight queries + diagnostics mapping). [REQ-94]
- System-font-only typography for editor and GUI (no font file paths). [REQ-92], [DEC-65]
- TOML keybindings with `primary` abstraction, multi-bindings, and deterministic conflict resolution. [REQ-96]
- New top-level menu “Configuration” to open `./config/` in the IDE explorer (scoped root). [REQ-95], [DEC-66]
- UTF-8-only TOML IO enforcement (no encoding option). [REQ-98]

## [SCOPE-03] Out-of-Scope (v0.3)
- External filesystem watcher for config changes (no reload on outside edits). [REQ-91]
- CLI/env overrides for config (nice-to-have; not MVP). [DEC-14] (existing)
- Project-scoped `.arcanee/*` layered settings/keybindings model (replaced by `./config/`). [DEC-62]
- Shipping bundled font files or accepting font file paths. [REQ-92]
- Perfect 1:1 replication of upstream theme token taxonomies; IDE token set remains stable and owned by ARCANEE. [REQ-93], [REQ-94]

## [REQ-01] Core Baseline
inherit from blueprint_v0.2_ch1.md

## [REQ-02] Observability
inherit from blueprint_v0.2_ch1.md

## Requirements (v0.3 additions)
> These requirements are additive; all v0.2 requirements remain in effect unless explicitly superseded by a [DEC].

- [REQ-90] Repo-root `config/` with optional TOML files: `color-schemes.toml`, `editor.toml`, `gui.toml`, `keys.toml`; missing files fall back to built-in defaults.
- [REQ-91] Hot-apply on save (IDE saves only): debounce → parse → validate → apply; invalid fields fall back per-field; app remains usable.
- [REQ-92] Fonts are system-installed only: family/weight/style allowed; **no font file paths**; invalid font selection falls back to last-known-good (or deterministic system fallback) with Problems diagnostics.
- [REQ-93] Unified theming: active scheme is the single source of truth for editor UI colors, syntax token colors, and ImGui style colors.
- [REQ-94] Tree-sitter highlighting for `.nut` and `.toml`, using a stable IDE-owned token set and capture→token mapping.
- [REQ-95] “Configuration” top-level menu:
  - “Open Config Folder” opens `./config/` in IDE explorer scoped to that root.
  - Optional “New Config File” creates missing config files populated with commented defaults.
- [REQ-96] Keybindings in `config/keys.toml`: multiple bindings per action, `primary` modifier abstraction, conflict detection with deterministic **last definition wins**.
- [REQ-97] Problems integration: config parse/validation issues produce structured diagnostics (file, line/col if available, severity, message, key path).
- [REQ-98] UTF-8-only TOML IO: reject invalid encoding at IO boundary; surface diagnostics; do not add an encoding option.

## [DEC-62] Settings/Keybindings Model Replacement
- v0.3 uses `./config/*.toml` and replaces the v0.2 blueprint’s layered user/project settings model; no legacy path migration is required unless later discovered in code.

## [DEC-65] System Font Discovery Backends
- Use platform-native discovery/backends (DirectWrite/CoreText/fontconfig) and degrade gracefully on failure.

## [DEC-67] Config Root Resolution
- Primary: process working directory `./config/`.
- Compatibility fallback: `exe_dir/config` with a warning diagnostic if used.

## [ARCH-12] Hot-Apply Pipeline Budget
- Parse/validate off-thread; apply on main thread via atomic snapshot swap.
- Debounce to avoid repeated resource rebuilds (fonts/ImGui style) during rapid edits.

## [SCOPE-10] Budgets (Performance / Determinism)
inherit from blueprint_v0.2_ch1.md

## [TEST] Traceability for v0.3 additions
Each new [REQ] maps to ≥1 test (implementation details in Ch8 + test plan in Ch7/Ch8).

- [REQ-90] → [TEST-39]
- [REQ-91] → [TEST-39]
- [REQ-92] → [TEST-42]
- [REQ-93] → [TEST-40]
- [REQ-94] → [TEST-40]
- [REQ-95] → [TEST-43]
- [REQ-96] → [TEST-41]
- [REQ-97] → [TEST-39]
- [REQ-98] → [TEST-39]

## [TEST-39] Config Parse/Validate/Fallback + Diagnostics
- Load defaults + merge `config/*.toml` if present.
- Unknown keys warn; invalid values fallback per-field.
- Parse errors non-fatal; last-known-good snapshot retained.
- UTF-8 invalid IO rejected with diagnostics.
- Diagnostics emitted to Problems with file + best-effort range and key path.

## [TEST-40] Unified Theme Application (Editor + Tree-sitter + ImGui)
- Switching active scheme updates:
  - editor UI palette
  - syntax token palette (capture→token→scheme.syntax)
  - ImGui style colors via derived mapping (+ optional overrides)
- Deterministic mapping verified via golden/serialized style snapshot.

## [TEST-41] Keybinding Determinism + `primary` Mapping
- `primary` expands to Ctrl (Win/Linux) and Cmd (macOS).
- Conflicts produce warnings; last definition wins deterministically.
- Multiple bindings per action supported.

## [TEST-42] System Fonts Only + Safe Rebuild
- Font path keys ignored + Problems diagnostic.
- Missing/unavailable family falls back to deterministic system fallback.
- Failed atlas/resource rebuild preserves last-known-good font and UI remains usable.

## [TEST-43] Configuration Menu Opens Config Explorer Root
- Invoking “Configuration → Open Config Folder” ensures `./config` exists and sets file explorer root to the config folder without changing project root.

