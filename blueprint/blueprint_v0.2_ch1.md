# Blueprint v0.2 — Chapter 1: Scope & North Star

## 1.1 Objective
inherit from /blueprint/blueprint_v0.1_ch1.md

## 1.2 In-Scope / Out-of-Scope
inherit from /blueprint/blueprint_v0.1_ch1.md

### 1.2.1 v0.2 Additions (IDE-only “Full IDE”)
- [SCOPE-08] v0.2 adds a **boot-to-IDE “Full IDE”** experience implemented entirely inside the Workbench (IDE-only), per [CR-0043].
- [SCOPE-09] v0.2 defines the **workspace root** as `samples/<project>/` with per-project metadata under `samples/<project>/.arcanee/`.
- [SCOPE-10] v0.2 introduces IDE services (Parse/Search/Timeline/Tasks/LSP/DAP) with strict responsiveness rules ([REQ-76]).
- [SCOPE-11] Out of scope for v0.2 IDE: Git integration, terminal emulation, non-Squirrel LSP support, runtime feature expansion beyond Preview run/stop/restart controls.

## 1.3 North Star NFRs
inherit from /blueprint/blueprint_v0.1_ch1.md

### 1.3.1 v0.2 IDE NFR extensions
- [REQ-76] **Responsiveness rule**: UI thread does **render + input only**. All parse/search/snapshot/protocol I/O must run on background workers and deliver immutable results via a main-thread apply queue. All long work must be cancellable.
- [REQ-63] **Boot-to-IDE**: application boots into a dockspace with classic panes (Files left, Editor center, Preview right, Console bottom) and persists layout.
- [REQ-70] Preview embeds the **existing** runtime view; no new runtime features; Preview controls limited to Run/Stop/Restart + optional Restart-on-save.
- [REQ-13] Networking remains out of scope (no telemetry / downloads / multiplayer) for v1.0 and for the IDE in v0.2.

## 1.4 Budgets
inherit from /blueprint/blueprint_v0.1_ch1.md

### 1.4.1 v0.2 IDE budgets (initial)
- [DEC-34] **Initial IDE performance budgets**
  - Rationale: “Full IDE” adds multiple always-on services; without explicit budgets we risk regressions and unusable UX.
  - Alternatives:
    - (A) No explicit budgets until late stabilization.
    - (B) Only qualitative budgets (“fast enough”).
  - Consequences:
    - Adds measurable performance gates in Ch7/CI and micro-bench harnesses in Ch5/Ch8.
    - If budgets prove unrealistic on low-end devices, revise via CR + perf-regression policy (Ch9).

| Budget ID | Metric | Target | Measurement notes |
|---|---:|---:|---|
| [REQ-76] | Editor typing latency (p50) | ≤ 8 ms | From keypress to buffer+UI state updated (excluding vsync). |
| [REQ-76] | Editor typing latency (p95) | ≤ 16 ms | Same as above; includes selection rendering and caret. |
| [REQ-69] | tree-sitter incremental parse apply | ≤ 16 ms p95 | Per edit batch; highlight+fold+outline update computed off-thread, apply on main thread. |
| [REQ-68] | Workspace search “first results visible” | ≤ 200 ms p95 | For 10k small files or ~50 MB text workspace; streaming results. |
| [REQ-68] | Cancel propagation | ≤ 50 ms | From user cancel to workers halting + UI idle state. |
| [REQ-74] | Snapshot creation (background) | ≤ 500 ms p95 | For a “scripts-only” project (<5 MB text); UI must remain responsive. |
| [REQ-63] | Cold boot-to-IDE time | ≤ 2.5 s p95 | Release build, warm disk cache assumptions documented in perf harness. |

## 1.5 Observability Baseline
inherit from /blueprint/blueprint_v0.1_ch1.md

### 1.5.1 v0.2 observability additions (IDE)
- [REQ-77] IDE services (Parse/Search/Timeline/Tasks/LSP/DAP) must emit structured logs with **service + document/task/session IDs** and duration fields for key operations (parse batch, search batch, snapshot write, protocol request/response latency).
- [REQ-78] A per-frame debug overlay (dev-only) must expose queue depths and worker utilization for IDE services, without enabling any new runtime features.

## 1.6 v0.2 Feature Requirements (Full IDE)
> These are additive; existing requirements remain unchanged.

### 1.6.1 Project & Navigation
- [REQ-64] Project Explorer: file tree, quick filters, create/rename/delete, recent files.
- [REQ-65] Command Palette: unified entry point (open file/symbol, run task, toggle pane, create snapshot, debug controls).
- [REQ-66] Breadcrumb bar: project ▸ folder ▸ file navigation.

### 1.6.2 Editor Core
- [REQ-67] Text editing baseline: cut/copy/paste, multi-cursor, column selection, undo/redo (undo tree), find/replace in file.
- [REQ-68] Workspace search/replace: PCRE2 regex, streaming results, replace preview, cancellable.
- [REQ-69] Incremental syntax highlight + locals + injections via tree-sitter queries; Squirrel grammar; CST-driven folding + indentation folding; outline/symbols view.
- [REQ-80] Diagnostics display: underline + gutter markers + Problems list; source is LSP (if enabled) or local checks.

### 1.6.3 Preview & Inspector
- [REQ-70] Preview panel embeds existing runtime view; Run/Stop/Restart; optional Restart-on-save; optional FPS/status overlay.
- [REQ-81] Inspector tabs: Diagnostics, Docs, Search result detail.

### 1.6.4 Console & Tasks
- [REQ-71] Bottom console tabs: IDE Log Console, Task Output (problem matchers), optional REPL, Debug Console.
- [REQ-82] Tasks are defined in `samples/<project>/.arcanee/tasks.toml` and run with captured stdout/stderr (no terminal emulation).

### 1.6.5 Debugger & Language Services
- [REQ-72] Debugger UI + DapClient using DAP messages: breakpoints, call stack, variables, watches, debug console.
- [REQ-83] DAP baseline adapter is **ARCANEE runtime/Squirrel** (temporary or permanent) per user directive; must be usable for CI gating.
- [REQ-73] LspClient is in scope for **Squirrel only** (diagnostics and navigation hooks; rename/format optional later).

### 1.6.6 Timeline (Local History)
- [REQ-74] Automatic snapshots: before Run/Debug, before format/refactor hooks, periodic while dirty; plus manual labeled checkpoints; restore + diff.
- [REQ-84] Retention policy: keep **15** snapshots for “script” files and **3** snapshots for other files (file-type classification defined in Ch8).
- [REQ-85] Exclusions: Timeline must exclude temporary/rebuildable artifacts (build outputs, caches) via ignore patterns (defined in Ch8; enforced by TimelineStore).

### 1.6.7 Settings & Keybindings
- [REQ-75] Layered TOML settings (user + project), keybinding editor UI with conflict detection per context (Editor/Tree/Console/Debugger), persist layout + open files + cursor positions.
- [REQ-86] Default keybinding map is **VSCode-like** (documented mapping table in Ch8).


