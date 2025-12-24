# Upgrade Intake — “Full IDE” for Fantasy Console (IDE-only)

## 0) Snapshot
- **Repo:** ARCANEE
- **Current Blueprint:** `blueprint_v0.1_ch0..ch9.md` (repo version `0.1.0`)
- **Intake Type:** Mode (C) — conforming ZIP + vNEXT delta
- **Date (Europe/Rome):** 2025-12-24
- **Proposed Change Request:** [CR-0043] (next available ID after examples already present in `/blueprint/knowledge/`)

---

## 1) Observed Facts (from repo)
### 1.1 Repository & build
- Existing build is CMake + presets (`CMakePresets.json`) with a Ninja-first posture; repo targets Linux/Windows/macOS per [REQ-17], toolchain baselines per [REQ-52].
- Dependency fetching is currently via CMake `FetchContent` (not vcpkg/conan manifest), with DiligentCore + DiligentTools pinned to `v2.5.5` in root `CMakeLists.txt`. This matches the “pinned deps” intent in [REQ-61] but the governance mechanism is “FetchContent pins” rather than a package manager.
- Tooling scripts exist under `/tools/ci/` including `check_no_temp_dbg.py` (supports [TEMP-DBG] enforcement intent).

### 1.2 Current Workbench (“minimal editor”)
- A Workbench exists and is part of the product scope ([REQ-01], [REQ-28], Ch8).
- Current Workbench UI implementation is in `src/app/Workbench.{h,cpp}` and uses:
  - ImGui headers + SDL backend (`imgui_impl_sdl.h`) and Diligent’s ImGui integration (via DiligentTools).
  - The editor is currently an `InputTextMultiline`-style string buffer (no multi-cursor, no undo tree, no workspace search, no tree-sitter, no LSP/DAP).
  - Windows currently used are “Project Browser”, “Code Editor”, “Log Console” (no docking layout / no classic panes yet).
- Workbench requirements in v0.1 include “code editor (minimum)”, “basic debugger (minimum viable)”, and profiler overlay ([REQ-28], [REQ-58], [REQ-59]) with smoke tests ([TEST-09], [TEST-24], [TEST-25]).

### 1.3 Runtime/Preview integration
- Workbench initialization already receives `RenderDevice`, `Window`, and `Runtime*`, so embedding the existing runtime view as an IDE Preview panel appears structurally feasible without adding new runtime features (aligns with the delta constraint: IDE-only).

---

## 2) Compliance Snapshot (v0.1 repo health vs blueprint conventions)
### 2.1 Mode-C predicates (repo readiness)
- `/blueprint/` exists and contains:
  - `blueprint_v0.1_ch0..ch9.md`, `decision_log.md`, `walkthrough.md`, `implementation_checklist.yaml`, `/blueprint/knowledge/`, `/blueprint/cr/` ✅
- `/src/` exists and is non-empty; root `CMakeLists.txt` exists ✅
- Therefore: qualifies for Mode (C) “upgrade” flow.

### 2.2 Notable compliance considerations (to keep stable during vNEXT)
- **IDs:** Existing IDs run up to:
  - Requirements: max seen `[REQ-62]`
  - Decisions: max seen `[DEC-26]`
  - Tests: max seen `[TEST-26]`
  - Build: max seen `[BUILD-06]`
  - Architecture: max seen `[ARCH-10]`
  - Concurrency: max seen `[CONC-03]`
  - Memory: max seen `[MEM-02]`
  - Versioning: max seen `[VER-09]`
- **Public API:** `/include/` is absent; this is consistent with existing decision [DEC-19] (v1.0 is app-first; SDK optional).
- **TEMP-DBG enforcement:** script exists; confirm CI wiring in vNEXT when IDE work introduces heavy churn (must keep “no TEMP-DBG” gate effective).
- **Known risk:** The upcoming “Full IDE” feature is a large change to Tooling (Ch8) and will introduce new persistence (Timeline) and new protocol clients (LSP/DAP); these must be reflected in vNEXT blueprint + gates.

---

## 3) Delta Summary — vNEXT proposal (“Full IDE”, IDE-only)
### 3.1 User-facing delta (requested)
Add a boot-to-IDE, classic pane IDE (Files left, Editor center, Preview right, Console bottom) built entirely on Dear ImGui docking branch, with:
- Project Explorer + Symbols/Outline + Command Palette + Breadcrumbs.
- Editor with multi-cursor, column selection, undo tree, file and workspace search/replace (PCRE2), tree-sitter incremental highlight/folding/outline with Squirrel grammar and query files.
- Preview panel embedding the existing runtime view (no new runtime features) with Run/Stop/Restart and optional “Restart on save”.
- Console bottom tabs: logs, task output (problem matchers), optional REPL, debug console.
- Protocol clients: DAP (debugger UI) and optional LSP client.
- Local Timeline history with automatic snapshots + restore + diff, stored in SQLite + zstd + xxHash.
- Settings + keybinding editor UI; TOML layered (user + project), layout persistence.

### 3.2 Reserved new Blueprint IDs for vNEXT (proposed; to be formally introduced in [CR-0043])
> These are **reservations** to avoid renumbering drift. Final wording will land in vNEXT blueprint chapters + decision logs.

#### Requirements (new)
- [REQ-63] IDE boots directly into a **dockspace** with classic panes (Files/Editor/Preview/Console) and persistent layout.
- [REQ-64] Project Explorer: file tree, filters, create/rename/delete, recent files.
- [REQ-65] Command Palette: unified action entry point (open file/symbol, tasks, layout toggles, snapshots, debug controls).
- [REQ-66] Breadcrumb bar: project ▸ folder ▸ file navigation.
- [REQ-67] Editor core: multi-cursor, column selection, undo/redo (undo tree), find/replace in file.
- [REQ-68] Workspace search/replace: regex (PCRE2), streaming results, previewed replace, cancellable.
- [REQ-69] ParseService: tree-sitter incremental parse + query-based highlight/locals/injections, syntax folding + outline for Squirrel.
- [REQ-70] Preview panel embeds existing runtime view with Run/Stop/Restart + optional Restart-on-save; no new runtime features.
- [REQ-71] Console & Tasks: log console + task output with problem matchers (file:line), optional REPL, debug console.
- [REQ-72] DAP client: breakpoints/call stack/variables/watches + debug console using JSON messages.
- [REQ-73] LSP client (optional, architected): JSON-RPC semantic features (diagnostics, goto/refs/rename/format optional).
- [REQ-74] Timeline: automatic + manual snapshots, restore, diff; SQLite metadata + zstd blobs keyed by xxHash.
- [REQ-75] Settings + keybindings: layered TOML (user + project), keybinding UI with conflict detection and context scoping; persist open files + cursor + docking layout.
- [REQ-76] Performance & responsiveness: UI thread does render/input only; parse/search/snapshot/protocol I/O are background with cancellability and main-thread apply queue.

#### Architecture (new)
- [ARCH-11] Tooling/IDE subsystem decomposition: UIShell, ProjectSystem, DocumentSystem, TextBuffer, ParseService, SearchService, TaskRunner, TimelineStore, LspClient, DapClient with explicit ownership and threading.

#### Decisions (new; candidates)
- [DEC-27] ImGui **docking branch** + multi-viewport policy and how it is integrated (via DiligentTools fork vs direct ImGui submodule vs FetchContent).
- [DEC-28] TextBuffer internal data structure (piece-table vs rope vs gap-buffer hybrid) + undo tree representation and perf tests.
- [DEC-29] Timeline retention + storage layout + encryption/no-encryption policy (local data) + crash consistency.
- [DEC-30] LSP server discovery/config model (project TOML config; per-language; lifecycle; sandbox constraints).
- [DEC-31] DAP transport + adapter discovery/config + “at least one adapter” compliance strategy (real external adapter vs shipped mock adapter tool).
- [DEC-32] Task execution sandbox model (cwd/env/path allowlist; problem matcher configuration; Windows quoting rules).

#### Build (new)
- [BUILD-07] Add and pin new dependencies: tree-sitter(+grammar), PCRE2, SQLite, zstd, xxHash, tomlplusplus, nlohmann/json, spdlog (if not already present as a pinned dep), and integrate them consistently across platforms.

#### Tests (new)
- [TEST-27] IDE boot smoke: dock layout created, panes present, layout persistence roundtrip.
- [TEST-28] TextBuffer correctness: multi-cursor + column selection + undo tree invariants across randomized edit sequences.
- [TEST-29] SearchService: PCRE2 regex search streaming + cancel correctness; replace preview correctness.
- [TEST-30] ParseService: incremental tree-sitter updates preserve highlight spans + outline stability across edits (golden tests).
- [TEST-31] TimelineStore: snapshot/restore/diff correctness + zstd/xxHash dedup hit-rate sanity.
- [TEST-32] TaskRunner: capture stdout/stderr, problem matcher detects file:line and emits clickable diagnostics model.
- [TEST-33] DAP client: connect/handshake/stack/vars flow against a deterministic test adapter.
- [TEST-34] Settings/keybindings: layered TOML merge + keybinding conflict detection rules.

---

## 4) Impact Matrix (what changes where)
| Area | Existing v0.1 | vNEXT delta | Risk/Notes |
|---|---|---|---|
| UI shell | Simple ImGui windows | Full dockspace + multi-viewport + command palette | Layout persistence + focus/shortcut routing complexity |
| Editor | String-based multiline editor | New TextBuffer + multi-cursor + undo tree + find/replace | Requires correctness/perf tests; large rewrite |
| Parsing/highlight | Best-effort (minimal) | tree-sitter incremental parse + query-based highlight/folding/outline | New deps + query assets + incremental edit plumbing |
| Search | In-file only (basic) | Workspace search/replace with PCRE2 streaming + cancel | Requires concurrency + cancellation correctness |
| Preview | Workbench interacts with runtime | Embed existing runtime view as preview pane | Must not add runtime features; just host/controls |
| Console | Log console | Task output, problem matchers, optional REPL, debug console | Regex matchers need configuration and tests |
| Debug | MVP debugger in Ch8 | DAP client UI + protocol manager | Must define adapter strategy and testing |
| History | None | Timeline snapshots with SQLite+zstd+xxHash + diff | Persistence schema + storage location decisions |
| Settings | Ad-hoc | TOML layered + keybinding editor + layout persistence | UX + schema evolution policy needed |

---

## 5) Gaps / Risks (to be resolved via Sharp Questions or vNEXT decisions)
- **DAP adapter requirement ambiguity:** “Working against at least one adapter” can mean “external adapter installed on system” or “adapter shipped with repo as /tools”. This impacts DoD, CI, and user setup. (Targets [DEC-31], [TEST-33].)
- **ImGui docking branch integration:** Current ImGui appears sourced via DiligentTools; docking branch requirement may necessitate switching integration strategy or ensuring Diligent’s ImGui is docking-enabled. (Targets [DEC-27], [BUILD-07].)
- **Tree-sitter grammar & query assets:** Need a clear source/version pin for `tree-sitter-squirrel` and query file locations inside the repo. (Targets [REQ-69], [BUILD-07], [TEST-30].)
- **Project model & tasks TOML:** Need a canonical workspace root definition, ignore rules, and where tasks/settings live. (Targets [REQ-64], [REQ-71], [REQ-75], [DEC-32].)
- **Timeline privacy/security:** Snapshot content may include secrets. Need policy for storage location, retention, and whether to exclude files/patterns. (Targets [DEC-29].)
- **Performance budgets for IDE:** Without budgets, regression gates may be weak. Need at least “typing latency” and “search responsiveness” targets. (Targets [REQ-76].)

---

## 6) Sharp Questions (answer in next turn; all are blocking unless we assume defaults)
1. **Target vNEXT version:** Should we target `v0.2.0` for this feature set (MINOR bump from `0.1.0`), or do you want a different SemVer plan for pre-1.0?
v0.2.0

2. **ImGui source of truth:** Must ImGui docking come from:
   - (A) DiligentTools’ ImGui integration (if docking-enabled), OR
   - (B) a direct ImGui docking branch dependency we integrate ourselves (and then bridge to Diligent), OR
   - (C) something else already present in the repo?
A

3. **Workspace root & project format:** What is the canonical project/workspace structure?
   - Is a “project” simply a folder under `samples/` today, and do we keep that, or introduce `.arcanee/` metadata under a chosen root?
a folder under samples/

4. **Tasks TOML location & schema:** Where should tasks live (example: `<workspace>/.arcanee/tasks.toml`) and what minimum schema do you want (name, command, args, cwd, env, problem matcher)?
`<workspace>/.arcanee/tasks.toml` keep schema as simple as possible for our needs

5. **DAP “at least one adapter”:** Which adapter should be considered the baseline for DoD?
   - (A) ship a small deterministic mock adapter under `/tools/` for CI + demo, OR
   - (B) integrate with a known external adapter (name it), OR
   - (C) something specific to Squirrel/runtime (even if temporary).
C

6. **LSP scope:** Is LSP required for the DoD milestone, or truly optional/phase-2?
   - If enabled, which language servers do you want to support first (Squirrel-only, or also others in the workspace)?
Squirrel only

7. **Timeline retention & exclusions:** Default retention policy (e.g., keep last N snapshots or last X days) and file exclusion rules (e.g., `*.png`, `*.wav`, `build/`, `*.bin`)?
15 snapshot for scripts and 3 snapshot for other files, exclude temporary (eg: rebuildable artifacts) files. 

8. **Encoding baseline:** Can we assume UTF-8 for text files (plus `\n`/`\r\n` normalization), or must we support mixed encodings?
UTF-8 for text files (plus `\n`/`\r\n` normalization)

9. **Keybinding defaults:** Do you want a VSCode-like default map, an Emacs/Vim mode, or a minimal custom set (with later user customization)?
VSCode-like

10. **Non-goals confirmation:** Confirm that runtime/render/audio features must not change for this milestone beyond exposing run/stop/restart controls in the IDE preview.
confirmed
