# Decision Log — ARCANEE

This log is append-only. IDs are stable.

## [DEC-01] Primary target user experience
- Status: **Accepted**
- Decision: ARCANEE is a fantasy console runtime that runs cartridges with deterministic stepping and an integrated Workbench.
- Rationale: aligns specs + README + v1.0 goal.
- Consequences: Workbench becomes first-class subsystem (Ch8).

## [DEC-02] Cross-platform intent
- Status: **Accepted**
- Decision: Support Linux/Windows/macOS for v1.0.

## [DEC-03] Rendering strategy intent
- Status: **Accepted**
- Decision: Multi-backend rendering via Diligent rather than raw Vulkan/Metal/DX12 in v1.0.

## [DEC-04] Public API boundary (C++ SDK)
- Status: **Accepted**
- Decision: v1.0 does not promise a stable C++ SDK ABI; “public API” is script/file formats + CLI.
- Consequences: `/include/` optional until SDK is required.

## [DEC-05] Dependency + supply-chain policy
- Status: **Accepted**
- Decision: FetchContent is allowed for v1.0, but all deps must be pinned and recorded; CI must be reproducible.

## [DEC-06] Sandbox/security posture
- Status: **Accepted**
- Decision: Treat cartridges as untrusted; enforce strict filesystem sandbox; validate bindings; impose limits.

## [DEC-07] Packaging/distribution
- Status: **Accepted**
- Decision: v1.0 ships as single-folder portable build.

## [DEC-08] v1.0 deliverable composition
- Status: **Accepted**
- Decision: runtime + workbench.

## [DEC-09] Target OS set
- Status: **Accepted**
- Decision: Linux + Windows + macOS.

## [DEC-10] GPU backend policy
- Status: **Accepted**
- Decision: Diligent is the primary renderer abstraction.

## [DEC-11] Graphics feature subset
- Status: **Accepted**
- Decision: 2D + 3D included in v1.0.

## [DEC-12] Determinism contract
- Status: **Accepted**
- Decision: determinism covers simulation + input only.

## [DEC-13] Threat model
- Status: **Accepted**
- Decision: untrusted downloadable cartridges; strict sandbox; never read/write outside cartridge scope.

## [DEC-14] Packaging target
- Status: **Accepted**
- Decision: portable single-folder build.

## [DEC-15] Dependency management preference
- Status: **Accepted**
- Decision: keep CMake FetchContent.

## [DEC-16] Scripting surface
- Status: **Accepted**
- Decision: Squirrel is the long-term scripting choice.

## [DEC-17] Networking
- Status: **Accepted**
- Decision: out-of-scope for v1.0.

## [DEC-18] Blueprint vs specs precedence
- Status: **Accepted**
- Decision: `/blueprint/` is normative; `/specs/` is reference.

## [DEC-19] `/include/` requirement
- Status: **Accepted**
- Decision: optional for v1.0 (no stable C++ SDK ABI in v1.0).

## [DEC-20] Diligent backend selection rule
- Status: **Accepted**
- Decision: Windows DX12 default; macOS Metal; Linux Vulkan; optional OpenGL fallback.

## [DEC-21] Error model
- Status: **Accepted**
- Decision: use `Status/StatusOr` style error returns (no exceptions as primary control flow).

## [DEC-22] Public API definition
- Status: **Accepted**
- Decision: v1.0 public API is script bindings + file formats + CLI.

## [DEC-23] Allocator strategy
- Status: **Accepted**
- Decision: FrameArena + ResourceHeap (tracked), to stabilize frame times.

## [DEC-24] GPU sync complexity
- Status: **Accepted**
- Decision: avoid multi-queue/multi-threaded rendering in v1.0.

## [DEC-25] Workbench UI stack
- Status: **Accepted**
- Decision: ImGui + docking for v1.0 workbench.

## [DEC-26] Hot reload strategy
- Status: **Accepted**
- Decision: scripts hot reload; assets best-effort; polling fallback watcher in v1.0.

---

### [DEC-27] ImGui docking branch via DiligentTools integration (source of truth)
- Rationale: user-selected; minimize integration churn; align with existing renderer tooling path.
- Alternatives: direct ImGui docking dependency; other UI framework.
- Consequences: docking/multi-viewport enablement is constrained to DiligentTools-supported path; avoid introducing standalone ImGui submodule.

### [DEC-28] TextBuffer internal representation = piece-table + line index + undo tree
- Rationale: fast edits without large memmoves; undo tree stores piece slices efficiently; deterministic batching for multi-cursor.
- Alternatives: rope; gap buffer.
- Consequences: provide immutable snapshots for background services; normalize multi-cursor batches back-to-front.

### [DEC-29] Timeline policy: retention + storage model + crash consistency
- Rationale: local “timeline” without Git; bounded growth; predictable restore/diff.
- Alternatives: Git integration; unlimited history; per-user store.
- Consequences: retention enforced deterministically; timeline failures must not block editing; crash consistency rules required.

### [DEC-30] LSP discovery/config model: Squirrel-only with project TOML control
- Rationale: limit scope; semantics optional but architected; configuration via `.arcanee/settings.toml`.
- Alternatives: require user-installed servers; multi-language first.
- Consequences: LSP is additive; tree-sitter remains always-on structural engine.

### [DEC-31] DAP baseline: ARCANEE runtime/Squirrel adapter
- Rationale: user-selected; enables deterministic CI gating against the repo’s own adapter.
- Alternatives: mock adapter tool; external adapter.
- Consequences: adapter must be usable in tests; debug hooks must not affect non-debug runtime behavior.

### [DEC-32] Tasks model: `<workspace>/.arcanee/tasks.toml` minimal schema + builtin matchers
- Rationale: “simple as possible” while meeting DoD (task list, run, output capture, clickable diagnostics).
- Alternatives: full VSCode tasks schema; external terminal integration.
- Consequences: extend later via CR; keep v0.2 schema stable and test-covered.

### [DEC-33] Introduce top-level `/cr/` directory alongside `/blueprint/`
- Rationale: align repo change workflow with CR artifacts; separate governance records from blueprint chapters.
- Alternatives: CRs only under `/blueprint/cr/`.
- Consequences: keep `/blueprint/` as source of truth; CI gates must locate CRs reliably.

### [DEC-34] Initial IDE performance budgets + CI enforcement
- Rationale: prevent UX regressions during large IDE expansion.
- Alternatives: no budgets until late; qualitative only.
- Consequences: add micro-bench gates ([TEST-35], [TEST-36]) and perf-regression policy in Ch9.

### [DEC-35] Timeline storage location: project-local `.arcanee/timeline/`
- Rationale: “history follows project” and is simplest for v0.2.
- Alternatives: per-user OS app-data dir; hybrid.
- Consequences: must be excluded from exports/packaging; ignore patterns required.

### [DEC-36] tree-sitter query assets location: `assets/ide/treesitter/<lang>/queries/*.scm`
- Rationale: centralized, packagable, version-pinned with repo.
- Alternatives: embed strings; place alongside grammar vendoring.
- Consequences: packaging must include assets; add asset verification target.

### [DEC-37] Dock layout persistence: per-project `.arcanee/layout.ini`
- Rationale: predictable across machines; aligns with per-project session state.
- Alternatives: per-user persistence; store in TOML.
- Consequences: CI must not depend on layout; tests use temp projects.

### [DEC-38] UIShell main-thread apply queue is the only mutation ingress
- Rationale: simplifies thread-safety and determinism; enforces [REQ-76].
- Alternatives: fine-grained locks; actor model.
- Consequences: define result packet schemas and a stable apply order.

### [DEC-39] Standard CancelToken across IDE services
- Rationale: “Cancel everywhere” correctness + unified testing.
- Alternatives: ad-hoc atomics; std::stop_token directly everywhere.
- Consequences: wrap/bridge stop_token as needed; require periodic checks in all long jobs.

### [DEC-40] Stable per-frame apply order for service result packets
- Rationale: avoids flicker; ensures decorations align with latest revisions.
- Alternatives: apply in arrival order.
- Consequences: services tolerate delayed apply; tests assume deterministic ordering.

### [DEC-41] IDE service errors use Status/Expected; no exceptions across boundaries
- Rationale: explicit failure handling + responsiveness.
- Alternatives: exceptions; error_code only.
- Consequences: define unified `Status`; test error paths and degrade-gracefully UI behavior.

### [DEC-42] LSP transport: shipped `arcanee_squirrel_lsp` tool over stdio JSON-RPC
- Rationale: standard LSP approach; clean boundary; testable.
- Alternatives: embed server; require external install.
- Consequences: add tools target + smoke tests; feature can be disabled via flag.

### [DEC-43] DAP baseline adapter: in-process DAP server component with abstract transport
- Rationale: current runtime + IDE in same process; easiest bring-up while preserving protocol contracts.
- Alternatives: adapter process (stdio); adapter process + IPC.
- Consequences: keep path open for external adapters later; [TEST-33] validates real DAP message flow.

### [DEC-44] Column selection defined in (line, Unicode-scalar column) space
- Rationale: predictable UX with multibyte UTF-8; matches mainstream editors.
- Alternatives: byte columns; grapheme clusters.
- Consequences: conversion logic required; include multibyte tests.

### [DEC-45] tree-sitter input materialization happens on worker thread per parse job
- Rationale: avoids UI-thread cost; matches [REQ-76].
- Alternatives: maintain always-materialized UI buffer.
- Consequences: materialization must be cancellable.

### [DEC-46] Workspace search buffering cap (default 10k hits) + truncation event
- Rationale: prevent memory spikes from pathological regex.
- Alternatives: unlimited; spill-to-disk.
- Consequences: UI shows truncation; tests cover cap behavior.

### [DEC-47] TimelineStore SQLite schema v1 + blob files
- Rationale: simplest list/restore/diff with dedup and pruning.
- Alternatives: blobs inside SQLite; chunked CAS.
- Consequences: crash consistency rules; orphan blob cleanup on open.

### [DEC-48] zstd compression level = 3 for snapshots
- Rationale: speed/ratio balance for frequent snapshots.
- Alternatives: level 1; higher levels.
- Consequences: perf budgets assume this default.

### [DEC-49] xxHash64 for dedup keys with collision safety handling
- Rationale: fast non-crypto; acceptable for local history with safeguards.
- Alternatives: xxHash32; BLAKE3.
- Consequences: on collision signals, store salted blob and record event.

### [DEC-50] Protocol (LSP/DAP) message size caps + ring buffer caps
- Rationale: prevent memory exhaustion from large protocol payloads.
- Alternatives: unlimited; adaptive.
- Consequences: tests validate caps; UI surfaces truncation/limits.

### [DEC-51] Task cancel escalation: graceful then hard kill after 500ms
- Rationale: avoid orphan tools while allowing flush.
- Alternatives: immediate kill; long grace.
- Consequences: tests simulate hung tasks; cancel must return promptly.

### [DEC-52] Feature flag policy: flags for bring-up; release presets enable all DoD IDE features
- Rationale: permits staged development without weakening DoD.
- Alternatives: no flags; plugin system.
- Consequences: CI asserts release-default flags; avoid silently shipping disabled features.

### [DEC-53] PCRE2 JIT: build-enabled where available; runtime optional
- Rationale: perf boost without hard requirement.
- Alternatives: require JIT; disable JIT entirely.
- Consequences: [TEST-29] must pass without JIT; perf harness logs status.

### [DEC-54] Nightly-only stress tests (release-blocking): [TEST-37], [TEST-38]
- Rationale: runtime/noise; nightly stability signal.
- Alternatives: always on PR; never.
- Consequences: release requires recent green nightly.

### [DEC-55] Script file types for retention policy
- Rationale: keep deeper history for actively edited text sources.
- Alternatives: `.nut` only.
- Consequences: retention tests include script and non-script fixtures.

### [DEC-56] Default IDE ignore patterns exclude rebuildable/temporary artifacts
- Rationale: reduce timeline noise + storage; align with user exclusion requirement.
- Alternatives: no defaults; explicit config only.
- Consequences: TimelineStore and ProjectSystem filters use these defaults; tests validate exclusions.

### [DEC-57] Command registry model (stable command names + enable predicates)
- Rationale: one action system for palette/menu/keybindings.
- Alternatives: separate systems.
- Consequences: supports headless command tests and deterministic bindings.

### [DEC-58] Keybinding context model (Editor/Tree/Console/Debugger/Global)
- Rationale: meaningful conflict detection and predictable resolution.
- Alternatives: global-only; per-window only.
- Consequences: keybinding editor UI must visualize conflicts; tests validate.

### [DEC-59] Built-in problem matcher set v0.2 (gcc_clang/msvc/generic)
- Rationale: meet DoD with minimal schema complexity.
- Alternatives: fully user-defined regex in TOML.
- Consequences: expand later via CR; [TEST-32] covers builtin matchers.

### [DEC-60] Snapshot scheduling: periodic 5 min while dirty + 30s debounce; always before Run/Debug
- Rationale: usability without excessive churn.
- Alternatives: every minute; manual-only.
- Consequences: tests use fake clock; triggers deterministic.

### [DEC-61] No secret redaction in v0.2 (exclusions + user awareness)
- Rationale: robust detection is complex; keep scope contained.
- Alternatives: integrate secret scanning/redaction.
- Consequences: document policy; follow-up CR if needed.

---


