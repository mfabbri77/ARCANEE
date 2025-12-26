<!-- _blueprint/v0.1/ch0.md -->

# Blueprint v0.1 — Chapter 0: Meta

## 0.1 Snapshot
- [META-01] **Repo**: ARCANEE
- [META-02] **Blueprint version**: `0.1` (tracks repo `0.1.0`; see Ch9 for SemVer policy)
- [META-03] **Date (Europe/Rome)**: 2025-12-24
- [META-04] **Mode**: ZIP migrate/repair (Mode B)

## 0.2 Scope Tags
- [SCOPE-03] BACKEND: `aaa_engine`
- [SCOPE-04] PLATFORM: `linux`, `windows`, `macos`
- [SCOPE-05] TOPIC: `gpu`, `persistence`, `security`, `tooling`

## 0.3 Problem Statement
- [REQ-01] The repository must ship **ARCANEE v1.0** as a **runtime + workbench** for running and developing “cartridges” (script + assets) with deterministic simulation and sandboxed I/O.

## 0.4 Non-Goals (v1.0)
- [REQ-13] Networking (multiplayer, downloads, telemetry) is **out of scope** for v1.0.
- [REQ-14] Deterministic rendering/audio output is **not required**; determinism is limited to simulation + input (see Ch1).

## 0.5 Source of Truth
- [ARCH-01] **This `/blueprint/` is the source of truth** for implementation and governance.
- [DEC-18] Specs in `/specs/` remain **supporting reference**, but any conflict is resolved in favor of `/blueprint/`.
  - Rationale: migration requires a single normative contract.
  - Alternatives: keep `/specs/` canonical.
  - Consequences: all new changes must update `/blueprint/` (via CRs).

## 0.6 Repo Baseline (Observed)
- [SCOPE-06] Present: `src/`, `specs/`, `samples/`, `cmake/`, `docs/session_notes/`
- [SCOPE-07] Missing (to be created): `/blueprint/`, `/tests/`, (optional) `/include/` public API boundary.

## 0.7 Primary Deliverables
- [DEC-08] **v1.0 deliverable**: **runtime + workbench**
  - Consequences: include editor, hot reload, debugging/profiling UI; see Ch8.

- [DEC-09] **v1.0 target OS**: Linux + Windows + macOS
  - Consequences: CI matrix + packaging rules; see Ch2/Ch7.

- [DEC-10] **GPU policy**: keep **Diligent** as primary renderer abstraction
  - Consequences: backend selection table + GPU rules; see Ch3/Ch6.

- [DEC-11] **Graphics subset**: **2D + 3D** in v1.0
  - Consequences: retained-model APIs + asset pipeline gates; see Ch3/Ch4.

- [DEC-12] **Determinism contract**: **simulation + input only**
  - Consequences: golden tests hash state/logs, not frames/audio; see Ch1/Ch6.

- [DEC-13] **Threat model**: **untrusted downloadable cartridges**
  - Consequences: strict filesystem sandbox, binding safety, limits; see Ch1/Ch5.

- [DEC-14] **Packaging**: **single-folder portable build** (v1.0)
  - Consequences: runtime layout, asset lookup rules; see Ch2/Ch7.

- [DEC-15] **Dependency mgmt**: keep **CMake FetchContent**
  - Consequences: reproducibility policy + pinned versions; see Ch7/Ch9.

- [DEC-16] **Scripting**: **Squirrel long-term**
  - Consequences: ABI surface for bindings + VM limits; see Ch4/Ch5.

- [DEC-17] **Networking**: out of scope for v1.0
  - Consequences: hard gate in CI to prevent accidental linkage; see Ch7.

## 0.8 Glossary
- [META-05] **Cartridge**: a packaged unit containing Squirrel scripts + assets + manifest.
- [META-06] **Workbench**: integrated IDE/editor/debugger/profiler for cartridges.
- [META-07] **Tick**: fixed simulation step (default 60Hz) independent of render rate.

## 0.9 ID & Change Control Policy
- [REQ-15] IDs are **append-only and never reused**. New IDs must be monotonic per prefix.
- [VER-01] All blueprint changes require a **CR** under `/blueprint/cr/CR-XXXX.md` (see Ch9).
- [REQ-16] Any temporary debug code must be marked `[TEMP-DBG]` and CI must fail if any remain.

## 0.10 Compliance Gates (must exist by v1.0)
- [TEST-01] `check_no_temp_dbg`: fail build if any `[TEMP-DBG]` markers exist.
- [TEST-02] `check_no_na_without_dec`: fail if any `N/A` exists without a `[DEC-XX]` justification.
- [TEST-03] `check_req_has_test_and_checklist`: fail if any `[REQ-*]` lacks ≥1 `[TEST-*]` and ≥1 checklist step reference.
- [TEST-04] `check_inherit_targets`: fail if any “inherit from …” references missing files/sections (used for vNEXT upgrades).


