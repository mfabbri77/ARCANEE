# Walkthrough — ARCANEE v0.1 Blueprint → v1.0 Implementation

This walkthrough is the recommended execution order. Each step references Blueprint IDs.

## Phase A — Establish Governance & Gates
1) Create `/blueprint/` structure and commit this snapshot.  
   Refs: [ARCH-01], [REQ-15], [VER-01]

2) Add CI “policy gates” targets:
   - `check_no_temp_dbg` [TEST-01]
   - `check_no_na_without_dec` [TEST-02]
   - `check_req_has_test_and_checklist` [TEST-03]
   Refs: [REQ-16]

3) Introduce `CMakePresets.json` + CTest presets.
   Refs: [REQ-53], [REQ-54], [TEST-20], [TEST-21]

## Phase B — Security & Determinism Foundations
4) Implement/verify VFS sandbox invariants:
   - canonicalize + deny traversal/absolute paths
   - deny symlink escape
   - restrict writes to save namespace only
   Refs: [REQ-22], [REQ-47]

5) Add determinism harness:
   - fixed seed, fixed dt, input snapshot recording
   - serialize sim state and hash
   Refs: [REQ-19], [REQ-21], [TEST-05]

## Phase C — Core Runtime + Script
6) Formalize `Status/StatusOr` and apply across subsystems.
   Refs: [DEC-21]

7) Harden Squirrel bindings:
   - validate args/types
   - convert errors to Status + script error
   Refs: [REQ-24], [TEST-07]

## Phase D — Render + Audio
8) Implement backend selection and ensure init works per platform:
   - Win DX12, macOS Metal, Linux Vulkan (OpenGL optional)
   Refs: [DEC-20], [REQ-32], [TEST-08]

9) Audio callback safety:
   - bounded queue from main thread to callback thread
   - no allocations in callback
   Refs: [REQ-40], [TEST-14]

## Phase E — Workbench v1.0 MVP
10) Integrate ImGui + docking and implement:
   - project browser + editor
   - run/stop/reload + script hot reload
   - log console + filter
   Refs: [REQ-28], [DEC-25], [DEC-26], [TEST-09], [TEST-13]

11) Add debugger MVP:
   - breakpoints + step/continue
   Refs: [REQ-58], [TEST-24]

12) Add profiler overlay MVP:
   - timers + alloc counters
   Refs: [REQ-59], [TEST-25]

## Phase F — Packaging & Release
13) Implement `package_portable` target creating the v1.0 folder layout and licenses bundle.
   Refs: [DEC-14], [REQ-33], [REQ-62], [TEST-22]

14) Run perf smoke baselines and set thresholds.
   Refs: [VER-09], [TEST-26]


