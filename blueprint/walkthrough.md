# Walkthrough (v0.2) — Full IDE (IDE-only) Upgrade

## 0) Scope
This walkthrough implements [CR-0043] targeting **v0.2.0**, adding the “Full IDE” feature set ([REQ-63..89]) while preserving existing runtime behavior (Preview is control-plane only) ([REQ-70], [REQ-88]).

**Primary rule:** UI thread does render + input only; everything else runs in background and publishes immutable result packets to the UIShell main-thread apply queue ([REQ-76], [DEC-38], [DEC-40]).

---

## 1) Repo and workspace conventions
1. Workspace root is `samples/<project>/` ([SCOPE-09]).
2. Per-project metadata lives in `samples/<project>/.arcanee/` ([REQ-82]):
   - `tasks.toml` (Tasks) ([REQ-82], [DEC-32])
   - `settings.toml` (Settings layering) ([REQ-75])
   - `keybindings.toml` (Keybindings overrides) ([REQ-75])
   - `session.toml` (open files + cursors) ([REQ-75])
   - `layout.ini` (ImGui layout) ([DEC-37])
   - `timeline/` (TimelineStore data) ([DEC-35])
3. tree-sitter query assets live at:
   - `assets/ide/treesitter/squirrel/queries/{highlights,locals,injections}.scm` ([REQ-69], [DEC-36])

---

## 2) Milestone plan (v0.2.0 DoD mapping)

### Milestone A — Scaffolding + build + gates
**Goal:** Create IDE module boundaries and wire CI gates before feature work explodes.

Steps:
1. Create internal library target `arcanee_ide` (or repo-equivalent) containing:
   - `src/ide/UIShell.*` ([API-07])
   - `src/ide/ProjectSystem.*` ([API-08])
   - `src/ide/DocumentSystem.*` ([API-09])
   - `src/ide/TextBuffer.*` ([API-10])
   - `src/ide/ParseService.*` ([API-11])
   - `src/ide/SearchService.*` ([API-12])
   - `src/ide/TaskRunner.*` ([API-13])
   - `src/ide/TimelineStore.*` ([API-14])
   - `src/ide/LspClient.*` ([API-15])
   - `src/ide/DapClient.*` ([API-16])
2. Add/pin dependencies per [BUILD-07] (FetchContent pins consistent with repo policy):
   - tree-sitter + tree-sitter-squirrel ([REQ-69])
   - PCRE2 ([REQ-68])
   - SQLite + zstd + xxHash ([REQ-74])
   - tomlplusplus ([REQ-75])
   - nlohmann/json ([REQ-72], [REQ-73])
   - spdlog (IDE logs) ([REQ-77])
3. Ensure ImGui docking branch is enabled via **DiligentTools ImGui integration** ([DEC-27]).
4. Add asset verification target `arcanee_assets_verify` ensuring query files are staged for runtime ([BUILD-09]).
5. Ensure CI gates are active for:
   - no `[TEMP-DBG]` ([TEST-01])
   - no `N/A` without `[DEC]` ([TEST-02])
   - every new [REQ-63..89] has ≥1 [TEST] + checklist step ([TEST-03])
   - inherit targets valid ([TEST-04])

Acceptance:
- `arcanee_assets_verify` fails if any query file missing ([BUILD-09]).
- PR CI runs [TEST-27..36] and the gate scripts (Ch7).

---

### Milestone B — Boot-to-IDE dockspace + session persistence
**Goal:** The app boots into the “Full IDE” shell and persists layout + open session.

Steps:
1. Implement UIShell dockspace with classic panes:
   - Project Explorer (L) ([REQ-64])
   - Editor (C) ([REQ-67])
   - Preview (R) ([REQ-70])
   - Console (B) ([REQ-71])
   - Optional Inspector tabs (Diagnostics/Docs/Search detail) ([REQ-81])
2. Implement `MainThreadQueue` and per-frame drain, applying result packets in fixed order ([DEC-38], [DEC-40]).
3. Layout persistence:
   - use ImGui `.ini` persistence, defaulting to `.arcanee/layout.ini` ([DEC-37])
4. Session persistence:
   - store open files + cursor positions in `.arcanee/session.toml` ([REQ-75])
5. Command Palette:
   - implement command registry + palette UI entry `Ctrl/Cmd+P` ([REQ-65], [DEC-57], [REQ-86])
6. Breadcrumb bar:
   - show `project ▸ folder ▸ file` and click navigation ([REQ-66])

Acceptance:
- [TEST-27] passes: pane existence + layout/session roundtrip + palette opens.

---

### Milestone C — ProjectSystem + DocumentSystem baseline
**Goal:** Real workspace navigation and file opening/saving with UTF-8 normalization.

Steps:
1. ProjectSystem:
   - open workspace root `samples/<project>` ([SCOPE-09])
   - build file tree model + filter string ([REQ-64])
   - implement create/rename/delete ([REQ-64])
2. DocumentSystem:
   - open/save/close, track dirty state ([REQ-67])
   - UTF-8 text baseline and EOL normalization policy ([REQ-87])
   - autosave tick hooks (non-blocking) ([REQ-76])
3. Recent files list persisted in `session.toml` ([REQ-64], [REQ-75])

Acceptance:
- Included in [TEST-27] integration coverage; file ops unit tests.

---

### Milestone D — TextBuffer (multi-cursor, column selection, undo tree)
**Goal:** Replace the minimal editor text control with a real editing core.

Steps:
1. Implement `TextBuffer` as piece-table + line index + undo tree ([DEC-28]).
2. Implement:
   - cut/copy/paste ([REQ-67])
   - multi-cursor ops (add caret/selection, multi-insert) ([REQ-67])
   - column selection rectangle per Unicode-scalar columns ([DEC-44], [REQ-67])
   - undo/redo with branching undo tree ([REQ-67])
   - in-file find/replace (plain + regex via PCRE2 when enabled) ([REQ-67])
3. Provide `TextSnapshot` immutable snapshots for background services ([MEM-04]).
4. Integrate EditorPane rendering using buffer snapshots + selection state (no blocking calls).

Acceptance:
- [TEST-28] passes: property tests for multi-cursor, column selection, undo invariants, UTF-8 multibyte cases.

---

### Milestone E — ParseService (tree-sitter highlight/fold/outline)
**Goal:** Always-on incremental structural engine.

Steps:
1. Integrate tree-sitter core + Squirrel grammar (pinned) ([REQ-69], [BUILD-07]).
2. Implement ParseService:
   - background parse jobs; cancellable ([REQ-76], [DEC-39], [DEC-45])
   - load query files from `assets/ide/treesitter/...` ([DEC-36])
   - compute highlight spans + folds + outline items tagged with `DocRevision` ([REQ-69])
3. Wire results to editor decorations and Symbols/Outline pane ([REQ-69]).
4. Ensure stale revision discard logic in UIShell ([REQ-69]).

Acceptance:
- [TEST-30] passes: golden tests for highlights/folds/outline + incremental edit stability.

---

### Milestone F — Workspace Search/Replace (PCRE2 streaming + cancel)
**Goal:** Fast, cancellable workspace search/replace with preview.

Steps:
1. Implement SearchService:
   - compile PCRE2 patterns; enable JIT where available but optional at runtime ([DEC-53])
   - enumerate files under workspace respecting ignore patterns ([DEC-56])
   - stream `Hit/Progress/Done/Canceled/Error` events to UI via main-thread queue ([REQ-68], [DEC-38])
   - enforce hit cap 10k and emit truncation event ([DEC-46])
2. Implement Search UI:
   - query editor (regex toggle, case toggle)
   - results list streaming
   - replace preview: show proposed changes before applying ([REQ-68])
   - cancel button always available ([REQ-68], [REQ-76])

Acceptance:
- [TEST-29] passes: streaming, cancel latency, replace preview correctness, cap behavior.

---

### Milestone G — Tasks + Console (problem matchers)
**Goal:** Bottom console with tasks defined in TOML and clickable diagnostics.

Steps:
1. Implement `tasks.toml` parsing (tomlplusplus) with minimal schema ([DEC-32], [REQ-82]).
2. TaskRunner:
   - spawn process, capture stdout/stderr (no terminal emulation) ([REQ-82])
   - cancellation with escalation policy (graceful → hard kill after 500ms) ([DEC-51])
3. Implement builtin problem matchers:
   - `gcc_clang`, `msvc`, `generic_file_line` ([DEC-59])
4. Console tabs:
   - IDE Log Console (spdlog sink) ([REQ-71], [REQ-77])
   - Task Output (stdout/stderr + problem events) ([REQ-71])

Acceptance:
- [TEST-32] passes: capture + matchers emit file:line diagnostics.

---

### Milestone H — TimelineStore (snapshots + restore + diff)
**Goal:** Local history timeline (not Git), bounded retention, exclusions.

Steps:
1. Implement TimelineStore:
   - SQLite metadata + blob store directory (`.arcanee/timeline/`) ([DEC-35], [DEC-47])
   - zstd compress snapshots (level 3) ([DEC-48])
   - xxHash64 dedup keys with collision safety handling ([DEC-49])
   - crash consistency: write blob then insert metadata transaction; cleanup orphans on open ([DEC-47])
2. Implement triggers ([REQ-74], [DEC-60]):
   - before Run
   - before Debug start
   - periodic while dirty (5 min) with per-file debounce (30s)
   - manual checkpoint with label
3. Implement retention ([REQ-84]) and exclusions ([REQ-85], [DEC-56]):
   - keep 15 snapshots for scripts and 3 for non-scripts
   - exclude rebuildable artifacts by default ignore patterns
4. Implement Timeline UI:
   - list snapshots per file
   - diff view between selections ([REQ-74])
   - restore selected snapshot (becomes new doc revision)

Acceptance:
- [TEST-31] passes: snapshot/restore/diff + retention (15/3) + exclusions + orphan cleanup simulation.

---

### Milestone I — DAP Debugger UI + ARCANEE adapter baseline
**Goal:** Protocol-shaped debugger UI driven by DAP semantics, with ARCANEE runtime/Squirrel adapter baseline.

Steps:
1. Implement DapClient:
   - message routing and session state (breakpoints/stack/vars/watch, debug console) ([REQ-72])
   - enforce message size caps ([DEC-50])
2. Implement baseline ARCANEE adapter as in-process DAP server component with abstract transport ([DEC-43], [REQ-83]).
3. Implement Debugger panes:
   - Breakpoints list
   - Call stack
   - Variables
   - Watch expressions
   - Debug console (evaluate + output) ([REQ-71], [REQ-72])
4. Wire breakpoints persistence to `.arcanee/debug.toml` (or `session.toml` section) ([REQ-89]).
5. Ensure debug hooks do not alter non-debug runtime behavior; hooks active only during session ([REQ-83], [CONC-12]).

Acceptance:
- [TEST-33] passes: deterministic sample hits breakpoint, stack/vars/evaluate work, continue completes.

---

### Milestone J — LSP (Squirrel-only) + diagnostics
**Goal:** Optional-but-in-scope LSP for Squirrel-only to provide diagnostics (and baseline nav hooks).

Steps:
1. Implement `arcanee_squirrel_lsp` tool (stdio JSON-RPC) per [DEC-42].
2. Implement LspClient:
   - start/stop server per workspace
   - didOpen/didChange notifications
   - diagnostics events routed to Problems list + editor underlines ([REQ-80], [REQ-73])
   - enforce message size caps ([DEC-50])
3. Ensure LSP is feature-flagged but enabled in release presets ([BUILD-08], [DEC-52]).

Acceptance:
- [TEST-34] passes: layered TOML + keybindings conflict detection + LSP smoke emits at least one diagnostic event.

---

## 3) Preview pane integration (IDE-only constraint)
Steps:
1. Embed the existing runtime view into PreviewPane.
2. Add Run/Stop/Restart buttons and optional Restart-on-save toggle ([REQ-70]).
3. Ensure these controls do not change runtime behavior when idle and do not add new runtime features ([REQ-88]).

Acceptance:
- [TEST-27] smoke-level check for pane + controls wiring (runtime tests remain existing suite).

---

## 4) Performance and regression gates
Steps:
1. Implement micro-bench harness tests for:
   - typing latency budget enforcement ([DEC-34], [TEST-35])
   - snapshot write budget enforcement ([DEC-34], [TEST-36])
2. Add stress tests to nightly:
   - search/edit cancel stress ([TEST-37], [DEC-54])
   - timeline snapshot stress ([TEST-38], [DEC-54])

Acceptance:
- PR CI runs [TEST-35..36] and they pass.
- Nightly runs [TEST-37..38] and must be green before release tag ([VER-16]).

---

## 5) Developer commands (runnable)
> Replace preset names with repo’s actual preset identifiers; these commands are referenced by checklist steps.

- Configure + build (Release): [BUILD-13]
  - `cmake --preset <release-preset>`
  - `cmake --build --preset <release-build-preset>`
- Run tests: [BUILD-14]
  - `ctest --preset <test-preset> --output-on-failure`
- Run perf gates only: [BUILD-15]
  - `ctest --preset <test-preset> -R "(TEST-35|TEST-36)" --output-on-failure`
- Verify IDE assets staged: [BUILD-16]
  - `cmake --build --preset <release-build-preset> --target arcanee_assets_verify`

---

## 6) Release readiness (v0.2.0)
A v0.2.0 tag is allowed only if ([VER-16]):
- PR CI is green including [TEST-27..36] and gate scripts.
- Nightly stress tests [TEST-37], [TEST-38] have been green within 7 days.
- No `[TEMP-DBG]` markers exist.
- Every [REQ-63..89] is covered by ≥1 [TEST] and ≥1 checklist step.

