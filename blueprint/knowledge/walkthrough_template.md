# walkthrough.md Template (Normative)
This template defines the canonical structure for `/blueprint/walkthrough.md`, the execution plan that drives implementation.  
It is designed to be used alongside `implementation_checklist.yaml` and must remain consistent with Blueprint IDs.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this template → project-specific constraints.

---

## 1) Overview
- **Blueprint Version:** vX.Y
- **Target Release:** vA.B.C
- **Repo Name:** <repo>
- **Primary Goal:** one sentence
- **Quality Gates:** list the mandatory gates (format, no TEMP-DBG, tests, sanitizers, etc.)
- **How to Run (Quickstart):** commands using presets

Example (replace with project commands):
- `cmake --preset dev && cmake --build --preset dev`
- `ctest --preset dev --output-on-failure`
- `cmake --build --preset dev --target check_no_temp_dbg`

---

## 2) Phases
Each phase contains steps. Each step must include:
- **Step ID** (e.g., `P1.S1`)
- **Refs**: Blueprint IDs (e.g., `[BUILD-01]`, `[API-03]`)  
- **Artifacts**: concrete files/targets created/modified
- **Commands**: runnable commands for build/test/verify
- **Definition of Done (DoD)**: objective pass/fail criteria
- **Notes**: optional

**Rule:** Steps must align with `implementation_checklist.yaml` tasks (milestones and tasks should be derivable from phases/steps).

---

## 3) Phase 0 — Repo Bootstrap & Tooling
### P0.S1 — Initialize repository structure
- **Refs:** [ARCH-XX], [BUILD-01]
- **Artifacts:** `/src`, `/include/<proj>`, `/tests`, `/tools`, `/blueprint`
- **Commands:**
  - `cmake --preset dev`
  - `cmake --build --preset dev`
- **DoD:**
  - Configure/build succeeds on primary platform
  - `compile_commands.json` generated (if required)
- **Notes:** N/A

### P0.S2 — Add quality gate targets
- **Refs:** [TEMP-DBG], [BUILD-01]
- **Artifacts:** `/tools/check_no_temp_dbg.*`, CMake target `check_no_temp_dbg`, `format_check`
- **Commands:**
  - `cmake --build --preset dev --target check_no_temp_dbg`
  - `cmake --build --preset dev --target format_check`
- **DoD:**
  - Gates exist and fail appropriately when violated

---

## 4) Phase 1 — Core API Skeleton
### P1.S1 — Define public API skeleton
- **Refs:** [API-01], [API-02], [API-03]
- **Artifacts:** `/include/<proj>/*.h(pp)`, export macros header
- **Commands:**
  - `cmake --build --preset dev`
- **DoD:**
  - Public headers compile
  - No backend headers leaked into public API (unless intended)

### P1.S2 — Implement error handling baseline
- **Refs:** [API-03], [REQ-XX], [TEST-XX]
- **Artifacts:** `src/<proj>/error.*`, tests for error mapping
- **Commands:**
  - `ctest --preset dev --output-on-failure`
- **DoD:**
  - Error behavior matches policy; tests added and passing

---

## 5) Phase 2 — Core Implementation (Hot Path)
### P2.S1 — Implement memory/allocator strategy
- **Refs:** [MEM-01], [MEM-02]
- **Artifacts:** allocator code + tests
- **Commands:**
  - `ctest --preset asan --output-on-failure`
- **DoD:**
  - No leaks; allocator correctness tests pass

### P2.S2 — Implement concurrency primitives (if applicable)
- **Refs:** [CONC-01], [CONC-02], [TEST-XX]
- **Artifacts:** thread pool/task system code + stress tests
- **Commands:**
  - `ctest --preset tsan --output-on-failure` (where supported)
- **DoD:**
  - Stress tests pass; no data races under TSan (where supported)

---

## 6) Phase 3 — Backends (Vulkan/Metal/DX12) (If applicable)
Repeat a consistent step pattern per backend.

### P3.VK.S1 — Vulkan backend init + capability capture
- **Refs:** [ARCH-XX], [REQ-XX], [TEST-XX]
- **Artifacts:** `src/backends/vulkan/*`, smoke test
- **Commands:**
  - `ctest --preset dev --output-on-failure`
- **DoD:**
  - Instance/device creation passes; capabilities logged

### P3.MTL.S1 — Metal backend init + offscreen smoke
- **Refs:** [ARCH-XX], [TEST-XX]
- **Artifacts:** `.mm` files under metal backend, offscreen test
- **Commands:**
  - `ctest --preset dev --output-on-failure`
- **DoD:**
  - Offscreen render smoke passes; no Obj-C leaks into core

### P3.DX12.S1 — DX12 backend init + debug layer (dev)
- **Refs:** [ARCH-XX], [TEST-XX]
- **Artifacts:** dx12 backend code, init tests
- **Commands:**
  - `ctest --preset dev --output-on-failure`
- **DoD:**
  - Device creation passes; errors mapped properly

---

## 7) Phase 4 — Python Bindings (If requested)
### P4.S1 — Bindings skeleton + packaging
- **Refs:** [PY-01], [PY-02], [PY-03]
- **Artifacts:** `src/python/*`, `python/` package files
- **Commands:**
  - `python -m pytest -q` (or via CTest label)
- **DoD:**
  - Import works; minimal API callable; wheels strategy documented

### P4.S2 — Zero-copy and ownership tests
- **Refs:** [PY-03], [TEST-XX]
- **Artifacts:** pytest tests for buffer protocol/lifetime
- **Commands:**
  - `python -m pytest -q`
- **DoD:**
  - Zero-copy is verified and safe; no crashes on GC

---

## 8) Phase 5 — Hardening, Sanitizers, and CI
### P5.S1 — Sanitizer clean on primary platform
- **Refs:** [BUILD-04], [TEST-XX]
- **Artifacts:** sanitizer presets, CI config
- **Commands:**
  - `ctest --preset asan --output-on-failure`
  - `ctest --preset ubsan --output-on-failure`
- **DoD:**
  - Sanitizers pass; failures are actionable

### P5.S2 — CI pipeline operational
- **Refs:** [BUILD-XX], [TEST-XX]
- **Artifacts:** CI config files
- **Commands:**
  - CI run passes on declared matrix
- **DoD:**
  - All quality gates are blocking

---

## 9) Phase 6 — Release Readiness
### P6.S1 — Documentation and versioning artifacts
- **Refs:** [VER-01], [VER-04], [VER-05]
- **Artifacts:** `CHANGELOG.md`, `MIGRATION.md` (if required), blueprint snapshot for release
- **Commands:**
  - `cmake --build --preset release`
  - `ctest --preset release --output-on-failure` (if defined)
- **DoD:**
  - Release notes complete; SemVer rules satisfied

### P6.S2 — Hygiene sweep
- **Refs:** [TEMP-DBG]
- **Artifacts:** none (cleanup)
- **Commands:**
  - `cmake --build --preset dev --target check_no_temp_dbg`
- **DoD:**
  - Zero TEMP-DBG markers; all gates pass

---

## 10) Mapping to Implementation Checklist
- Ensure each walkthrough step maps to a `TASK-XX.YY` entry in `implementation_checklist.yaml`.
- If a walkthrough step has no matching task, add it to the checklist (or justify its omission).

---

## 11) Notes
- Keep the walkthrough concise and action-oriented.
- Update this file as part of CRs that change scope or execution steps.
