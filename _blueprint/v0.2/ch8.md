<!-- _blueprint/v0.2/ch8.md -->

# Blueprint v0.2 — Chapter 8: Tooling (Workbench / Full IDE)

## 8.1 Tooling Scope
inherit from /blueprint/blueprint_v0.1_ch8.md

### 8.1.1 v0.2 Tooling Upgrade: “Full IDE” (IDE-only)
- [REQ-63] Boot-to-IDE dockspace with classic panes: Files (L), Editor (C), Preview (R), Console/Tasks (B).
- [REQ-64] Project Explorer + recent files/projects + file ops.
- [REQ-65] Command Palette is the primary entry point to IDE actions.
- [REQ-66] Breadcrumb bar navigation.
- [REQ-67] Editor baseline: multi-cursor, column selection, undo tree, in-file find/replace.
- [REQ-68] Workspace search/replace (PCRE2), streaming + cancellable + replace preview.
- [REQ-69] tree-sitter incremental structural engine for highlight/folding/outline via query files (Squirrel).
- [REQ-71] Bottom console: IDE logs + task output + optional REPL + debug console.
- [REQ-72] DAP client UI (breakpoints/stack/vars/watches/evaluate) + baseline ARCANEE DAP adapter ([REQ-83]).
- [REQ-73] LSP client (Squirrel-only) for diagnostics and navigation hooks.
- [REQ-74] Timeline local history (SQLite + zstd + xxHash) with restore + diff.
- [REQ-75] Settings + keybindings in TOML (layered user+project), VSCode-like defaults, conflict detection, layout/session persistence.

---

## 8.2 UIShell Layout, Docking, and Persistence
- [DEC-27] ImGui docking branch is enabled and sourced via **DiligentTools ImGui integration** (source of truth).
- [REQ-63] On boot, UIShell must create:
  - `ProjectExplorerPane` (left)
  - `EditorPane` (center)
  - `PreviewPane` (right)
  - `ConsolePane` (bottom, tabbed)
  - optional `InspectorPane` (right tabs: Diagnostics/Docs/Search detail) ([REQ-81])

### 8.2.1 Layout persistence
- [DEC-37] Persist docking layout to `samples/<project>/.arcanee/layout.ini` by default.
- [REQ-75] Persist IDE session state (open files + cursor positions) to:
  - `samples/<project>/.arcanee/session.toml` (project-local)
- [TEST-27] Layout + session roundtrip must be tested in a temp project directory (CI-safe).

---

## 8.3 Project Explorer, Recent Files, Breadcrumbs
### 8.3.1 Workspace root and metadata
- [SCOPE-09] Workspace root is `samples/<project>/`.
- [REQ-82] Project metadata lives under `samples/<project>/.arcanee/`.

### 8.3.2 Explorer behaviors
- [REQ-64] Project Explorer supports:
  - tree view (dirs/files)
  - quick filters (substring; optional glob later)
  - create/rename/delete (filesystem ops)
  - “recent files” list (bounded, e.g., 50 entries; stored in `session.toml`)
- [REQ-66] Breadcrumb bar shows `project ▸ folder ▸ file`, with clickable segments.
- [TEST-27] UI integration test covers pane visibility and basic navigation state wiring.

---

## 8.4 Command Palette (Primary Command Surface)
- [REQ-65] Palette actions include at minimum:
  - open file, open recent, open symbol (outline item)
  - run task, cancel task
  - toggle panes, reset layout
  - create timeline checkpoint, open timeline restore UI
  - debug controls (start/stop/continue/step, toggle breakpoint)
- [DEC-57] **Command registry model**
  - Decision: commands are registered at startup with:
    - stable string command name (e.g., `file.open`, `search.workspace`, `debug.continue`)
    - optional shortcut binding (contextual)
    - enable/disable predicate (based on current IDE state)
  - Rationale: enables palette + menu + keybinding to share one action system.
  - Alternatives: separate menu actions and palette actions.
  - Consequences:
    - Palette filtering is pure string match on command names + display labels.
    - Tests can invoke commands headlessly by name to validate invariants (used in [TEST-27], [TEST-28], [TEST-31], [TEST-33]).

---

## 8.5 Editor UX (TextBuffer + Decorations)
### 8.5.1 UTF-8 and EOL normalization
- [REQ-87] **File text normalization**
  - Decision (requirement): DocumentSystem loads files as UTF-8; if file is invalid UTF-8, open fails with recoverable error and no crash; EOL is normalized in-memory while preserving original EOL style on save (tracked per document).
  - Tests:
    - Add a unit test under [TEST-28] suite for invalid UTF-8 handling and EOL roundtrip.

### 8.5.2 Multi-cursor and column selection
- [REQ-67] Multi-cursor: add caret, add selection, expand selection by word/line, multi-insert.
- [DEC-44] Column selection rectangle is defined in `(line, Unicode-scalar col)` space.
- [TEST-28] Property tests include multibyte UTF-8 and column edits.

### 8.5.3 In-file find/replace
- [REQ-67] In-file find/replace is implemented using the same regex engine backend as workspace search when regex mode is enabled (PCRE2); plain substring mode is allowed and may bypass PCRE2 for speed.
- [TEST-28] In-file replace scenarios included as deterministic unit tests.

### 8.5.4 Decorations and diagnostics overlays
- [REQ-80] Diagnostics display:
  - underline in editor
  - gutter markers
  - Problems list pane
- [REQ-69] tree-sitter highlight spans drive syntax coloring; diagnostics overlays are a separate layer applied after highlights (Ch3 apply order [DEC-40]).
- [TEST-30] ensures highlight spans are stable; [TEST-34] ensures LSP diagnostics enter Problems list.

---

## 8.6 ParseService (tree-sitter) and Query Assets
- [REQ-69] tree-sitter is “always-on” for structure and incremental updates.
- [DEC-36] Query files are located at:
  - `assets/ide/treesitter/squirrel/queries/highlights.scm`
  - `assets/ide/treesitter/squirrel/queries/locals.scm`
  - `assets/ide/treesitter/squirrel/queries/injections.scm`

### 8.6.1 Outline / Symbols pane
- [REQ-69] Symbols/Outline pane is derived from tree-sitter outline extraction and updates incrementally per document revision.
- [TEST-30] Golden tests include outline extraction for representative Squirrel sources.

### 8.6.2 Folding
- [REQ-69] Folding sources:
  - indentation folding (fast heuristic)
  - syntax-node folding from CST (preferred where available)
- [TEST-30] Golden tests cover fold ranges for known fixtures.

---

## 8.7 Workspace Search/Replace (PCRE2)
- [REQ-68] Workspace search:
  - regex mode (PCRE2) + optional JIT (Ch7 [DEC-53])
  - streaming results to UI as they arrive
  - cancellable with visible cancel control
  - replace with preview (dry-run view before apply)
- [DEC-46] Per-session hit buffer cap (default 10k) prevents memory spikes.
- [TEST-29] validates: compile, scan, stream ordering, cancel latency, replace preview correctness.

---

## 8.8 Tasks and Problem Matchers
- [REQ-82] Tasks are defined in `samples/<project>/.arcanee/tasks.toml`.
- [DEC-32] Minimal schema (v0.2):
  - `[[task]] name`, `cmd`, optional `args`, optional `cwd`, optional `problem`
  - no terminal emulation; capture stdout/stderr

### 8.8.1 Built-in problem matchers
- [DEC-59] **Problem matcher set v0.2**
  - Decision: ship a small builtin set keyed by `problem`:
    - `gcc_clang`: `path:line:col: (warning|error): message`
    - `msvc`: `path(line,col): (warning|error) Cxxxx: message`
    - `generic_file_line`: `path:line: message`
  - Rationale: covers common build outputs and “clickable file:line” DoD.
  - Alternatives: fully user-defined regex per task in TOML (more flexible, more UI complexity).
  - Consequences:
    - v0.2 TOML stays simple; user can choose builtin matcher.
    - Later, add advanced schema via CR (would be v0.3+).
- [TEST-32] Must include at least one fixture validating each builtin matcher emits correct diagnostics.

---

## 8.9 Preview Pane (Runtime View Embedding)
- [REQ-70] Preview pane embeds the existing console runtime view; adds only:
  - Run / Stop / Restart controls
  - optional Restart-on-save
  - optional FPS/status overlay
- [REQ-88] Preview controls must not change runtime determinism/perf behavior when idle; Run/Stop/Restart are control-plane only.
- [TEST-27] Ensures preview pane exists and controls are wired (smoke-level); runtime functional tests remain as in v0.1.

---

## 8.10 Console Pane (Logs, Task Output, REPL, Debug Console)
- [REQ-71] Tabs:
  - IDE Log Console (from spdlog sinks)
  - Task Output (TaskRunner stdout/stderr + problem matches)
  - REPL (optional; can be OFF by feature flag; no DoD dependency)
  - Debug Console (DAP evaluate + output)

### 8.10.1 IDE Log Console
- [REQ-77] IDE services must log with service+correlation IDs and durations; UI provides filters by channel/level and text search.
- [TEST-27] Smoke verifies log console receives a known message on boot.

---

## 8.11 Timeline (Local History “Timeline”, not Git)
- [REQ-74] Automatic snapshots:
  - before Run
  - before Debug session start
  - before format/refactor commands (format/refactor may be phase-2; hooks exist)
  - periodic while dirty (timer)
- [REQ-84] Retention: 15 snapshots for scripts, 3 for other files.
- [REQ-85] Exclude temporary/rebuildable artifacts.

### 8.11.1 Script classification
- [DEC-55] **Script file type set**
  - Decision: treat the following as “scripts” for retention purposes:
    - `.nut` (Squirrel)
    - `.nute` (if used by project; otherwise keep for future compatibility)
    - `.toml` (project metadata scripts/config; optional but useful)
  - Rationale: keep deeper history for actively edited textual sources.
  - Alternatives: scripts = only `.nut`.
  - Consequences:
    - Classification is centralized in TimelineStore and reused by UI (Timeline filter).
    - Tests for retention enforcement include `.nut` and a non-script (e.g., `.png` placeholder) ([TEST-31]).

### 8.11.2 Exclusion patterns (temporary/rebuildable artifacts)
- [DEC-56] **Default IDE ignore patterns**
  - Decision: default exclusions include (relative to workspace root):
    - `build/`, `out/`, `bin/`, `obj/`, `.vs/`, `.idea/`, `.vscode/`
    - `.arcanee/timeline/` (for exports/packaging; TimelineStore itself includes it by design)
    - `*.tmp`, `*.log`, `*.pch`, `*.o`, `*.obj`, `*.dll`, `*.so`, `*.dylib`, `*.exe`
  - Rationale: aligns with “exclude rebuildable artifacts”; reduces noise and storage growth.
  - Alternatives: no defaults; require explicit configuration.
  - Consequences:
    - Exclusions are used by TimelineStore and ProjectSystem filters; user/project overrides can extend via `settings.toml`.
- [TEST-31] Must validate excluded paths do not generate snapshots even if edited or touched.

### 8.11.3 Snapshot triggers and debounce
- [DEC-60] **Snapshot scheduling**
  - Decision: periodic snapshots occur every 5 minutes while dirty, with a per-file debounce of 30 seconds (no snapshots more frequent than this unless “before Run/Debug”).
  - Rationale: meets usability goals without excessive disk churn.
  - Alternatives: snapshot every minute; snapshot only on explicit actions.
  - Consequences:
    - Tests simulate time with a fake clock to validate triggers ([TEST-31]).

---

## 8.12 Settings and Keybindings (TOML)
- [REQ-75] Settings are layered:
  - User-level: OS user data dir (e.g., `~/.config/arcanee/settings.toml` on Linux; platform-specific resolved in implementation)
  - Project-level: `samples/<project>/.arcanee/settings.toml`
  - Merge policy: project overrides user; unknown keys preserved (forward compatibility)
- [REQ-75] Keybindings are layered similarly:
  - user: `keybindings.toml`
  - project: `.arcanee/keybindings.toml`

### 8.12.1 Keybinding contexts and conflict detection
- [DEC-58] **Keybinding context model**
  - Decision: bindings are resolved by ordered contexts:
    1) Editor
    2) ProjectTree
    3) Console
    4) Debugger
    5) Global
  - Rationale: matches user expectation (“per context”), keeps conflict detection meaningful.
  - Alternatives: single global map or per-window maps only.
  - Consequences:
    - Keybinding editor UI shows conflicts within a context and across higher-priority contexts.
    - [TEST-34] includes conflict detection fixtures.

### 8.12.2 VSCode-like defaults
- [REQ-86] Default keybindings are VSCode-like. Minimum default mapping set for v0.2:
  - `Ctrl/Cmd+P`: Command Palette ([REQ-65])
  - `Ctrl/Cmd+Shift+P`: Command Palette (alternate)
  - `Ctrl/Cmd+O`: Open File
  - `Ctrl/Cmd+S`: Save
  - `Ctrl/Cmd+F`: Find in File
  - `Ctrl/Cmd+H`: Replace in File
  - `Ctrl/Cmd+Shift+F`: Workspace Search
  - `F5`: Start/Continue Debug
  - `Shift+F5`: Stop Debug
  - `F10`: Step Over
  - `F11`: Step Into
  - `Shift+F11`: Step Out
  - `Ctrl/Cmd+``: Toggle Console
- [TEST-34] validates defaults load and resolve correctly per context.

---

## 8.13 DAP Debugger UI + ARCANEE Adapter Baseline
- [REQ-72] Debugger panes:
  - Breakpoints list
  - Call stack
  - Variables
  - Watch expressions
  - Debug console (evaluate + output)
- [REQ-83] Baseline adapter is ARCANEE runtime/Squirrel (v0.2):
  - in-process DAP server component per Ch4 [DEC-43]
  - must support at minimum the message flows required by [TEST-33]
- [REQ-89] Breakpoints persist per project in `.arcanee/debug.toml` (or `session.toml` section) and are restored on boot.
- [TEST-33] uses a deterministic Squirrel sample under `samples/` and asserts:
  - breakpoint hit
  - stack trace non-empty
  - variables query returns at least one variable
  - evaluate returns expected value
  - continue resumes to completion

---

## 8.14 LSP (Squirrel-only)
- [REQ-73] LSP is in scope for Squirrel only.
- [DEC-42] LSP server is a shipped tool target `arcanee_squirrel_lsp` (stdio JSON-RPC).
- Minimum LSP feature set for v0.2:
  - diagnostics → Problems list + editor underlines ([REQ-80])
  - navigation hooks: go-to definition / references are optional and may be stubbed behind commands (phase 2)
- [TEST-34] LSP smoke test launches server, opens a known-bad Squirrel file, and receives at least one diagnostic.

---

## 8.15 IDE-only Security/Privacy Notes
- [REQ-85] Timeline excludes rebuildable artifacts and must not be bundled in cartridge exports or distributable packages (Ch2/Ch7).
- [DEC-61] **No secret redaction in v0.2**
  - Decision: v0.2 does not attempt to detect secrets in snapshots; rely on exclusions + user awareness.
  - Rationale: robust secret detection is complex; exclusions already reduce risk.
  - Alternatives: integrate secret scanners.
  - Consequences:
    - Document in README later; add a follow-up CR if secret scanning is required.

---

## 8.16 Tooling Test Mapping (DoD coverage)
- [TEST-27] Dockspace + panes + layout/session persistence.
- [TEST-28] TextBuffer correctness: multi-cursor, column selection, undo tree, UTF-8/EOL cases.
- [TEST-29] Workspace search/replace streaming + cancel + preview.
- [TEST-30] tree-sitter highlight/folding/outline with query assets.
- [TEST-31] Timeline snapshots + restore + diff + retention (15/3) + exclusions.
- [TEST-32] Tasks + output capture + builtin problem matchers.
- [TEST-33] DAP client + baseline ARCANEE adapter debug loop.
- [TEST-34] TOML layering + keybinding conflict detection + LSP smoke.


