<!-- _blueprint/upgrade_intake.md -->

# Upgrade Intake — ARCANEE v0.2 → v0.3 (proposed)
Feature: TOML-based configuration system (Editor + ImGui Dock GUI), unified theming, Tree-sitter TOML + Squirrel, keybindings in TOML, hot-apply on save.

---

## 1) Observed Facts (Repo Snapshot)
- [META-01] Repo: `ARCANEE` (ZIP import), CMake-based native app; `/src/` and `/tests/` exist; `/include/` is not present (app-only layout implied).
- [META-02] Blueprint snapshots present for v0.1 and v0.2: `blueprint_v0.2_ch0..ch9.md`, plus `decision_log.md`, `walkthrough.md`, `implementation_checklist.yaml`, and existing CRs including `[CR-0043]`.
- [DEC-15] UI stack is Dear ImGui. (Declared in v0.2 Ch0)
- [DEC-27] ImGui docking branch is enabled via DiligentTools ImGui integration. (Declared in v0.2 Ch8)
- [DEC-16] Config format is TOML (already chosen). (Declared in v0.2 Ch0)
- [REQ-69] Tree-sitter is the incremental structural engine for highlight/folding/outline (Squirrel). (Declared in v0.2 Ch1/Ch8)
- [REQ-75] Settings + keybindings exist in TOML with a **layered** user+project model:
  - user-level under OS user data dir (platform-resolved)
  - project-level under `.arcanee/settings.toml` and `.arcanee/keybindings.toml`
  - unknown keys preserved for forward compatibility. (Declared in v0.2 Ch8)
- [REQ-80] Diagnostics display includes editor underlines + gutter markers + Problems list pane. (Declared in v0.2 Ch8)
- [VER-12] TOML schema evolution rules exist (unknown keys preserved; removals/renames require deprecation/migration). (Declared in v0.2 Ch9)
- [BUILD-..] CMake uses FetchContent to bring in dependencies including `tree-sitter`, `tree-sitter-squirrel`, and `tomlplusplus` (observed in `CMakeLists.txt`). Pinning strategy needs explicit verification against dependency governance requirements.
- [REQ-02] Observability policy exists in the normative bundle; repo contains logging plumbing (`src/common/Log.h` observed). Some sources (e.g., IDE ParseService) currently print to `std::cerr`, which should be aligned to the approved logging path in future changes.
- [META-03] Tree-sitter Squirrel highlight query assets exist under `assets/ide/treesitter/squirrel/queries/*.scm`. There is no TOML grammar/query asset observed yet.

---

## 2) Compliance Snapshot (Mode C Readiness)
- [VER-11] Mode C discipline is referenced in v0.2 governance text (upgrade via CR + delta-only edits + `decision_delta_log.md`), but `decision_delta_log.md` is not present in the repo snapshot. This upgrade will introduce it as part of v0.3 artifacts.
- [BUILD-01] CI/quality gate scripts exist under `tools/ci/` including checks for:
  - no `[TEMP-DBG]`
  - no `N/A` without `[DEC-XX]`
  - every `[REQ]` has ≥1 `[TEST]` + checklist reference
  - inherit-target correctness.
- [BUILD-02] Dependency pinning and reproducibility needs explicit confirmation:
  - FetchContent repos appear referenced by URL; whether commits/tags are pinned must be verified and, if not pinned, addressed via governance (either in this CR or a follow-up CR).
- [ARCH-01] Repo layout deviates from the “library + public include” convention (no `/include/<proj>/`). If the project is intentionally app-only, this is acceptable but must remain explicit and enforced by blueprint decisions/tests.

---

## 3) Delta Summary (Requested Change vs v0.2)
This feature request expands and concretizes existing settings/keybinding direction ([REQ-75], [VER-12]) with:
- New **file-based configuration folder** (`config/`) and file split:
  - `config/color-schemes.toml`
  - `config/editor.toml`
  - `config/gui.toml`
  - `config/keys.toml`
- **Hot-apply on save** for all `config/*.toml` files with debouncing; non-fatal validation; per-field fallback.
- **Unified theming**: one active scheme drives editor UI colors, syntax token colors, and the full ImGui style.
- **Tree-sitter highlighting additions**:
  - `.nut` (Squirrel) highlighting (already in scope; must be confirmed as `.nut` mapping exists)
  - `.toml` highlighting via Tree-sitter TOML grammar + highlight queries.
- **Config editing inside the IDE**:
  - new “Configuration” menu
  - open/edit config folder via the IDE file explorer
  - Problems window diagnostics for parse/validation problems with line/column.
- **System-font-only enforcement** for editor and UI fonts (no font file paths accepted).
- Keybindings centralized in TOML with deterministic conflict resolution (last definition wins) + platform abstraction (`primary`).

---

## 4) Impact Matrix (What Changes, Where)
| Area | Primary Modules (expected) | Impact | Notes |
|---|---|---:|---|
| Config loader + schema validation | new `ConfigSystem` + TOML parsing/validation layer | High | Must provide per-field fallback + structured diagnostics. |
| IDE file explorer + menu | `UIShell` / menu wiring | Medium | Adds “Configuration” menu & scoped explorer root. |
| Problems diagnostics | existing diagnostics pipeline / Problems pane | High | Must map TOML parse + validation issues to file/range. |
| Theming | editor theme + ImGui style mapping | High | “Scheme is source of truth” across editor + GUI. |
| Tree-sitter highlighting | ParseService/highlight pipeline + assets | High | Add TOML grammar + queries; stable capture→token mapping. |
| Fonts | font selection/atlas rebuild | High | System font matching only; safe fallback on failure. |
| Keybindings | keybinding manager + action registry | Medium | New file shape + conflict rules + `primary` abstraction. |
| Build/deps | CMake deps for TOML TS grammar | Medium | Add tree-sitter-toml grammar dependency and assets. |

---

## 5) Gaps / Risks
- [META-09] **Settings model conflict risk**: v0.2 defines layered user+project TOML files ([REQ-75]); the request introduces a `config/` folder and a different precedence order. Requires an explicit migration/compatibility decision and test gates.
- [META-10] **Font discovery is platform-specific**: “system fonts only” implies enumerating/matching families across Windows/macOS/Linux and mapping weight/style; requires explicit dependency/tooling choice and graceful degradation.
- [META-11] **Hot-apply safety**: applying changes on save touches theme, keymaps, and font resources. Must be thread-safe relative to editor render loop and background services; failed font rebuild must keep last-known-good resources.
- [META-12] **Diagnostics with accurate ranges**: toml parser must expose line/column offsets; unknown keys and invalid values must still report usable ranges (or best-effort file-level diagnostics).
- [META-13] **Token taxonomy stability**: Tree-sitter captures vary by grammar; IDE-owned stable token set and capture→token mapping must be versioned and tested to avoid drift.
- [META-14] **Dependency governance**: adding tree-sitter TOML grammar and potentially font backends may increase dependency surface; must remain pinned and reproducible.

---

## 6) Proposed New IDs (Reserved for v0.3 Change)
> Allocation based on current maxima observed in repo snapshot: REQ max=89, DEC max=61, ARCH max=11, API max=16, BUILD max=17, TEST max=38, VER max=17, META max=8.

### New Requirements (draft)
- [REQ-90] Add `config/` directory with optional TOML files: `color-schemes.toml`, `editor.toml`, `gui.toml`, `keys.toml`; missing files fall back to built-in defaults.
- [REQ-91] Hot-apply on save for any `config/*.toml`: parse → validate → apply immediately with debouncing; invalid fields fall back per-field without preventing launch.
- [REQ-92] System-font-only enforcement: reject/ignore font path keys; editor/gui fonts select by family + (weight/style) with safe fallback.
- [REQ-93] Unified theming: active scheme is the single source of truth for editor UI colors, syntax token colors, and ImGui style colors (explicit or derived deterministic mapping).
- [REQ-94] Tree-sitter highlighting for `.nut` and `.toml`, with stable capture→token mapping and prompt re-theme repaint.
- [REQ-95] IDE “Configuration” menu: open config folder in file explorer (scoped root); optionally create missing config files with commented defaults.
- [REQ-96] Keybindings in `config/keys.toml`: multiple bindings per action, conflict detection, deterministic resolution (last wins), platform abstraction (`primary`).
- [REQ-97] Problems-window integration for config errors/warnings: file + range + severity + message + key path; non-fatal behavior.
- [REQ-98] UTF-8-only TOML IO: reject invalid encoding at IO boundary; surface diagnostics; no encoding option exposed.

### New Architecture / API / Decisions (draft)
- [ARCH-12] Config hot-reload pipeline: file-save event → debounce → parse/validate off-thread → main-thread apply queue (atomic snapshot apply).
- [API-17] Internal `ConfigSnapshot` contract + per-subsystem apply API (Theme/Fonts/Keymap) with rollback/last-known-good semantics.
- [DEC-62] Settings precedence & migration policy for `config/` vs existing layered `settings.toml`/`.arcanee/*` (must pick coexist/replace + deprecation path).
- [DEC-63] GUI theme derivation strategy: derived mapping from editor base colors (MVP) vs explicit `[scheme.gui]` block (optional override).
- [DEC-64] TOML parser diagnostics strategy: toml++ (already present) vs alternative; must support line/column ranges and non-fatal schema validation.
- [DEC-65] System font backend strategy per platform (DirectWrite/CoreText/fontconfig) and whether any new dependency is allowed.

### New Tests (draft)
- [TEST-39] Config parse/validate/apply: per-field fallback, unknown-key warnings, and stable diagnostics ranges for each `config/*.toml`.
- [TEST-40] Theme unification: changing active scheme updates editor UI + syntax token colors + ImGui style deterministically.
- [TEST-41] Keybindings: conflict detection + last-wins determinism; `primary` expansion per platform.
- [TEST-42] Font selection: rejects font paths; family matching fallback; failed atlas rebuild preserves previous valid font.

### Versioning
- [VER-18] Release target: ship as **v0.3.0** (MINOR bump from v0.2.0) with explicit compatibility notes for prior settings/keybindings.

---

## 7) Sharp Questions (Answers unblock CR + v0.3 blueprint deltas)
1) [DEC-62] Should `config/` **replace** the existing layered settings/keybindings paths from [REQ-75], or **coexist** (e.g., `config/` = user-level layer, `.arcanee/` = project-level layer)? If coexist, what is the final precedence order?
Replace, but check what is actually implemented in code. Not just the blueprint.

2) Where should `config/` live in a packaged app:
   - beside the executable (repo-root style),
   - under OS user data dir (current [REQ-75] pattern),
   - or both with a search order?
Put config/ in the repo root directory, for now.

3) For “hot-apply on save”, should reload trigger on:
   - saves performed inside the IDE only,
   - or also external filesystem modifications (watcher)?
IDE only.

4) Fonts: do we accept adding platform font discovery backends (DirectWrite/CoreText/fontconfig), or must we rely only on what is already in the repo’s dependency set?
Load fonts from OS, use the most appropriate platform font discovery backends for this task.
 
5) Keybinding action IDs: do you already have a canonical action registry list to freeze, or should v0.3 define the initial stable set (e.g., `file.save`, `command_palette.open`, `editor.copy`, etc.)?
v0.3 can define the stable step, but keep what is already defined in v0.2 as base.

6) Color schemes: should built-in reference schemes ship as:
   - hardcoded defaults only (no files created unless user requests),
   - or also as a generated `config/color-schemes.toml` template via “New Config File”?
hardcoded default file `config/color-schemes.toml` that the user can modify

7) `.nut` handling: confirm that Squirrel files are `.nut` in your project conventions (and not another extension), so the file-type routing is correct for Tree-sitter + highlighting.
I confirm.

