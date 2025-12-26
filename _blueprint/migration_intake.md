<!-- _blueprint/migration_intake.md -->

# Migration Intake — ARCANEE (Mode B: migrate/repair)

Date (Europe/Rome): **2025-12-24**

This document is Phase 1 output for ZIP migration/repair. It records **Observed Facts**, **Inferred Intent** (clearly marked), **Gaps/Risks**, a **Modernization Delta**, and **Sharp Questions** to unblock Phase 2.

---

## 0) Mode detection

- [META-01] **Archive name**: `ARCANEE.zip`
- [META-02] **Repository root (extracted)**: `ARCANEE/`
- [META-03] **Mode predicates**
  - P1 `/blueprint/` exists: **false**
  - P2 required `/blueprint/*` files exist: **false** (blocked by P1)
  - P3 `blueprint_vX.Y_ch*.md` exists: **false** (blocked by P1)
  - P5 minimal impl present (`CMakeLists.txt` + `/src/` + headers/tests): **partial**
    - `CMakeLists.txt`: **present**
    - `/src/`: **present**
    - `/include/`: **absent**
    - `/tests/`: **absent**
- [META-04] **Detected mode**: **B (ZIP migrate/repair)**

---

## 1) Observed Facts (repo truth)

### 1.1 Identity, licensing, and positioning

- [SCOPE-01] **Product description (README + code headers)**: “**Modern Fantasy Console**” runtime/engine named **ARCANEE**.
- [SCOPE-02] **License**: **GNU AGPLv3+** (root `LICENSE`; top-level `CMakeLists.txt` header).
- [META-05] **Project version (CMake)**: `0.1.0` (`project(ARCANEE VERSION 0.1.0 ...)`).

### 1.2 Scope tags (initial)

- [SCOPE-03] **BACKEND**: `aaa_engine` (fantasy console runtime/engine).
- [SCOPE-04] **PLATFORM**: `linux` + `macos` + `windows` (SDL2-based; README mentions all three).
- [SCOPE-05] **TOPIC**: `gpu` + `persistence` + `security` + `tooling` (renderer + cartridge/VFS/sandbox + Workbench in specs).

### 1.3 Repo layout (top-level)

- [SCOPE-06] **Top-level directories present**
  - `src/` runtime implementation
  - `specs/` extensive technical specifications (Chapters 1–12 + appendices)
  - `samples/` sample cartridges / scripts
  - `cmake/` custom CMake find modules + patch script
  - `docs/session_notes/` implementation tasks + verification notes
  - `.vscode/` editor config
- [SCOPE-07] **Not present**
  - `/blueprint/` (required by this blueprint system)
  - `/tests/` (no unit/integration test harness found)
  - `/include/` (no explicit public header boundary)

### 1.4 Build system and language

- [BUILD-01] **Build system**: CMake (root `CMakeLists.txt`; `cmake_minimum_required(VERSION 3.16)`).
- [BUILD-02] **Languages**: C and C++ (project declares `LANGUAGES C CXX`).
- [BUILD-03] **C++ standard**: **C++17 enforced** (commented as normative requirement in root `CMakeLists.txt`).
- [BUILD-04] **Primary build artifact**: executable target `arcanee` (`src/CMakeLists.txt`).
- [BUILD-05] **Warnings**: `-Wall -Wextra -Wpedantic -Werror` enabled for non-MSVC (per `src/CMakeLists.txt`).

### 1.5 Major subsystems (as implemented in `/src`)

- [ARCH-01] **Subsystem directories**
  - `app/` entrypoint + runtime loop (`main.cpp`, `Runtime.*`)
  - `platform/` SDL-based platform/window/time
  - `runtime/` cartridge + manifest (`Cartridge.*`, `Manifest.*`)
  - `vfs/` virtual filesystem (PhysFS integration)
  - `script/` Squirrel VM + native bindings (`script/api/*Binding.*`)
  - `render/` render device + 2D canvas (ThorVG) + present path
  - `audio/` audio device/manager + SFX mixer + module player (libopenmpt optional)
  - `input/` input snapshots/manager

### 1.6 Seed requirements (reserved IDs; will be refined in Phase 2)

- [REQ-01] **Core baseline**: the repo must build and run the `arcanee` executable from a clean checkout on supported platforms with deterministic tick scheduling per spec.
- [REQ-02] **Observability**: runtime must provide structured logging + diagnostics sufficient to debug cartridge/runtime issues (policy to be formalized in `/blueprint`).

### 1.7 Third-party dependencies (as configured today)

- [REQ-03] **SDL2**: required (`find_package(SDL2 REQUIRED)`).
- [REQ-04] **PhysFS**: required (custom `cmake/FindPhysFS.cmake`; linked via `${PHYSFS_LIBRARIES}` in `src/CMakeLists.txt`).
- [REQ-05] **Squirrel**: fetched and built via CMake `FetchContent` (patch step `cmake/patch_squirrel.sh`); linked as `squirrel_static`, `sqstdlib_static`.
- [REQ-06] **DiligentCore**: fetched via CMake `FetchContent` at **tag `v2.5.5`**; linked engines include:
  - `Diligent-GraphicsEngineVk-static`
  - `Diligent-GraphicsEngineOpenGL-static`
  - `Diligent-GraphicsTools`
- [REQ-07] **ThorVG**: fetched via CMake `FetchContent` at **tag `v0.15.6`**; linked as `thorvg_static`.
- [REQ-08] **libopenmpt**: optional system dependency via pkg-config; defines `HAS_LIBOPENMPT` when found.

### 1.8 Specs coverage (existing “source of truth” today)

- [REQ-09] The `specs/` directory contains:
  - Chapters: scope, architecture, cartridge+VFS+sandbox, execution+Squirrel, rendering (2D/3D), audio, input, workbench, error handling, performance budgets.
  - Appendices: public API reference, enums/constants mappings, example cartridges, module boundaries, rendering color space policy, debugger/step spec, packaging/export, clarifications.
- [REQ-10] Performance policy defaults found in specs (Chapter 12 excerpt):
  - `tick_hz = 60`
  - `max_updates_per_frame = 4`
  - `cpu_ms_per_update` soft budget ~ `2.0 ms` (workbench default), `2.0–4.0 ms` (player default)
  - VM memory caps: soft `64 MB`, hard `128 MB` (recommended)

---

## 2) Inferred Intent (explicitly inferred; validate)

- [DEC-01] **Primary target user experience**: a “fantasy console / modern home computer” runtime that runs **cartridges** (script + assets) with deterministic stepping and a Workbench IDE.
  - Rationale: README feature list + extensive specs set (workbench, debugger, deterministic input/time specs).
  - Alternatives: “runtime-only” player (no workbench), or “engine SDK” rather than console runtime.
  - Consequences: impacts packaging, UI/Workbench dependencies, sandboxing, and test strategy.

- [DEC-02] **Cross-platform intent**: Linux + Windows + macOS via SDL2 abstraction.
  - Rationale: README mentions Windows/macOS; SDL-based platform layer.
  - Consequences: CI matrix, GPU backend support, filesystem behavior differences.

- [DEC-03] **Rendering strategy intent**: rely on **Diligent Engine** to support multiple backends (Vulkan + OpenGL today) rather than writing raw Vulkan/Metal/DX12 immediately.
  - Rationale: build links both Diligent Vulkan and OpenGL engines.
  - Consequences: abstraction boundaries, GPU API policy, shader/toolchain requirements.

---

## 3) Gaps & Risks (migration blockers)

- [REQ-11] **Blueprint system missing**: no `/blueprint/` directory; no decision log; no walkthrough/checklist conforming to schema.
  - Risk: architecture/contracts aren’t governed; requirements/tests mapping is currently informal.

- [REQ-12] **No `/tests/` harness detected** (unit/integration/perf); validation appears to rely on `samples/` and manual walkthrough notes.
  - Risk: regressions in determinism, sandbox rules, rendering correctness, and audio timing will be hard to gate in CI.

- [DEC-04] **Public API boundary unclear**: no `/include/`; public surface appears split between specs and `script/api/*Binding.*`.
  - Risk: ABI/API churn, unclear ownership model, hard to version and test.
  - Consequences: we may need to formalize “public C++ API vs script API vs file-format API”.

- [DEC-05] **Dependency + supply-chain policy not yet codified**: FetchContent pulls several upstream Git repos at configure time.
  - Risk: non-reproducible builds (network, upstream availability), licensing obligations for redistribution, and slower CI.
  - Consequences: we’ll likely pin versions (already partially pinned), add caching/mirroring strategy, and produce a dependency manifest.

- [DEC-06] **Sandbox/security posture needs confirmation**: specs discuss VFS permissions/sandbox, but enforcement + threat model are unknown from repo scan alone.
  - Risk: cartridge escape via filesystem/environment, unsafe native bindings, or platform-dependent behavior.

- [DEC-07] **Packaging/distribution not implemented in repo**: there is an appendix on packaging/export, but no build targets/scripts observed.
  - Risk: unclear release process, install layout, and user experience.

---

## 4) Modernization Delta (what migration will change)

These are *planned* migration outputs for Phase 2 (subject to answers in Sharp Questions).

- [ARCH-02] **Introduce `/blueprint/` as the governing source of truth**:
  - Create `blueprint_v0.1_ch0..ch9.md` (Extended Blueprint Structure) by **mapping** content from `specs/` into blueprint chapters.
  - Create `/blueprint/decision_log.md` and seed it with the [DEC-*] items above.
  - Create `/blueprint/walkthrough.md` + `/blueprint/implementation_checklist.yaml` with every [REQ-*] mapped to ≥1 checklist step and ≥1 [TEST-*] (CI-gated).

- [BUILD-06] **Add reproducible build + CI gates**:
  - Add CMakePresets + CI matrix (Linux/macOS/Windows) with sanitizer configs where possible.
  - Add gates: fail on any `[TEMP-DBG]`, any `N/A` without `[DEC-*]`, missing req→test mapping, and missing inheritance targets.

- [TEST-01] **Add a minimal test harness**:
  - Create `/tests/` with CTest integration.
  - Add smoke tests running `samples/` cartridges deterministically (golden output / frame hash / log signature depending on policy).
  - Add unit tests around manifest parsing, VFS permissions, RNG/time determinism helpers, and binding argument validation.

- [ARCH-03] **Clarify module boundaries and ownership rules**:
  - Normalize include layout (`/include/arcanee/...` for public headers if needed) and keep internal headers in `/src`.
  - Document threading model for audio, render, VM, and IO.

