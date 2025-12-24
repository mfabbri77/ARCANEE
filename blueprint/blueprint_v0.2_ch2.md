# Blueprint v0.2 — Chapter 2: System Architecture (C4), Platform Matrix, Repo Map, Packaging

## 2.1 C4 — System Context
inherit from /blueprint/blueprint_v0.1_ch2.md

## 2.2 C4 — Containers
inherit from /blueprint/blueprint_v0.1_ch2.md

### 2.2.1 v0.2 Container Addendum: IDE Services (Tooling-only)
```mermaid
flowchart LR
  user([User]) -->|input| app[ARCANEE App\n(Runtime + Workbench)]
  app --> ui[UIShell\n(ImGui Dockspace)]
  ui --> proj[ProjectSystem]
  ui --> doc[DocumentSystem]
  doc --> tb[TextBuffer]
  doc --> parse[ParseService\n(tree-sitter)]
  doc --> search[SearchService\n(PCRE2)]
  ui --> tasks[TaskRunner]
  ui --> tl[TimelineStore\n(SQLite+zstd+xxHash)]
  ui --> lsp[LspClient\n(JSON-RPC)]
  ui --> dap[DapClient\n(DAP)]
  dap --> adapter[ARCANEE DAP Adapter\n(Runtime/Squirrel)]
  ui --> preview[Preview Pane\n(existing runtime view)]
```

## 2.3 C4 — Components (High-Level)

inherit from /blueprint/blueprint_v0.1_ch2.md

## 2.4 Platform Matrix

inherit from /blueprint/blueprint_v0.1_ch2.md

## 2.5 Repository Map (v0.2 additions)

inherit from /blueprint/blueprint_v0.1_ch2.md

### 2.5.1 Workspace & Project Metadata

* [SCOPE-09] **Workspace root**: `samples/<project>/`
* [REQ-82] **Project metadata folder**: `samples/<project>/.arcanee/`

  * `tasks.toml` (required for Tasks UI; minimal schema; see Ch8)
  * `settings.toml` (optional; project overrides; see Ch8)
  * `keybindings.toml` (optional; project-level overrides; see Ch8)
  * `layout.ini` (optional; per-project ImGui layout; see [DEC-37])
  * `timeline/` (TimelineStore data; see [DEC-35])

### 2.5.2 Tree-sitter Query Assets

* [REQ-69] Query files are shipped in-repo and loaded at runtime:

  * `queries/highlights.scm`
  * `queries/locals.scm`
  * `queries/injections.scm`
* [DEC-36] **Query file location**

  * Decision: store query files under `assets/ide/treesitter/<lang>/queries/*.scm` (starting with `squirrel/`).
  * Rationale: keeps IDE assets centralized, packagable, and version-pinned with the repo.
  * Alternatives:

    * (A) embed queries as C++ string literals.
    * (B) place queries adjacent to the grammar submodule/vendor.
  * Consequences:

    * Packaging must include `assets/ide/treesitter/**`.
    * Tests can use golden fixtures referencing these files ([TEST-30]).

## 2.6 Packaging & Distribution (v0.2 additions)

inherit from /blueprint/blueprint_v0.1_ch2.md

### 2.6.1 IDE Data Packaging Rules

* [REQ-70] Preview uses the existing runtime view; no runtime feature additions are packaged for v0.2.

* [DEC-35] **Timeline storage location**

  * Decision: store TimelineStore data **inside the project** at `samples/<project>/.arcanee/timeline/` by default.
  * Rationale: “local history follows the project” without per-user hidden state; simplest for v0.2 and aligns with project-local `.arcanee/` convention.
  * Alternatives:

    * (A) per-user OS app-data directory keyed by workspace path (better privacy separation; harder to move).
    * (B) hybrid: metadata local, blobs per-user.
  * Consequences:

    * `.arcanee/timeline/` must be excluded from exports and any “cartridge packaging” artifacts.
    * Add default ignore patterns so the repo doesn’t accidentally commit history (see [REQ-85]).
    * Large histories are bounded by retention ([REQ-84]) to prevent unbounded growth.

* [DEC-37] **Layout persistence strategy**

  * Decision: persist ImGui docking layout using ImGui `.ini` persistence, defaulting to per-project `samples/<project>/.arcanee/layout.ini`.
  * Rationale: user explicitly wants persisted layout + open files/cursors; storing per-project makes behavior predictable across machines.
  * Alternatives:

    * (A) per-user app-data dir.
    * (B) embed layout in `settings.toml`.
  * Consequences:

    * `.arcanee/layout.ini` becomes part of “IDE session state” and may be user-specific; treat as optional and ignorable (see Ch8).
    * CI should not depend on layout files; tests create temp projects ([TEST-27]).

## 2.7 Security & Trust Boundaries (v0.2 note)

inherit from /blueprint/blueprint_v0.1_ch2.md

### 2.7.1 IDE-only additions

* [REQ-85] TimelineStore must exclude rebuildable artifacts and any configured ignore patterns to reduce inadvertent persistence of generated data.
* [REQ-82] TaskRunner runs tools specified in `tasks.toml`; sandboxing rules (cwd/env allowlist, quoting) are defined in Ch8 and enforced by tests ([TEST-32]).
* [REQ-83] DAP adapter baseline is ARCANEE runtime/Squirrel; debug-only instrumentation must be gated by “session active” to avoid altering normal runtime behavior (see Ch6).

