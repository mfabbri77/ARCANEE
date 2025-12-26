<!-- _blueprint/v0.1/ch1.md -->

# Blueprint v0.1 — Chapter 1: Scope, Requirements, Budgets

## 1.1 Product Scope
- [REQ-01] Ship a cross-platform **fantasy console runtime + workbench** capable of running and developing cartridges.

## 1.2 Requirements (Normative)
### Platform & build
- [REQ-17] Support Linux/Windows/macOS with a single CMake-based build.
- [REQ-18] C++ standard: **C++17** for v1.0 (toolchain minimums defined in Ch7).

### Runtime lifecycle
- [REQ-19] Provide a deterministic fixed-timestep simulation loop with:
  - default `tick_hz = 60`
  - `max_updates_per_frame = 4` (spiral-of-death cap)
  - deterministic input snapshots (see Input).
- [REQ-20] Provide CLI flags for selecting cartridge, logging level, and workbench mode.

### Determinism contract (v1.0)
- [REQ-21] Determinism must cover **simulation + input only**:
  - same cartridge + inputs + seeds => same simulation state/log signature across runs on the same platform.
  - render frame pixels and audio samples are **excluded** from determinism guarantees (v1.0).

### Cartridge & sandbox security
- [REQ-22] Treat cartridges as **untrusted**. Enforce:
  - no read/write outside cartridge scope
  - path canonicalization and traversal prevention (`..`, absolute paths, drive letters)
  - deny symlink escape (platform-specific handling)
- [REQ-23] Implement per-cartridge resource limits:
  - VM memory hard cap (default 128MB)
  - maximum asset size per file and per cartridge (defaults defined in Ch5)
  - maximum log rate (anti-spam) (see Observability)
- [REQ-24] All native bindings must validate arguments and fail closed (no UB/crashes on bad script input).

### Rendering (2D + 3D)
- [REQ-25] Provide a 2D Canvas API (ThorVG-backed) and a 3D Scene API (Diligent-backed) sufficient to implement sample cartridges.
- [REQ-26] Render must present to a console framebuffer and support integer scaling + letterboxing rules.

### Audio
- [REQ-27] Provide SFX mixing and (optional) tracker module playback; if module playback missing, system must degrade gracefully.

### Workbench (v1.0)
- [REQ-28] Provide Workbench UI with:
  - project/cartridge browser and editor
  - run/stop/reload
  - debug console/log viewer
  - basic debugger (breakpoints + step) and profiler overlays (minimum viable)
- [REQ-29] Hot reload must be available in Workbench for script changes (asset reload can be best-effort).

### Observability (required)
- [REQ-02] Provide structured logging + diagnostics:
  - categories (runtime, vfs, script, render, audio, workbench)
  - severity levels
  - per-cartridge context (cartridge id/path)
  - build/version stamp
  - optional ring buffer for in-app log viewer

## 1.3 Budgets
- [REQ-30] Performance budgets (defaults; tunable but testable):
  - simulation tick: target ≤ 2.0ms/update typical, with hard cap behavior defined (drop/slowdown policy)
  - render: stable frame pacing, avoid unbounded allocations per frame
- [REQ-31] Startup budget: reach first frame ≤ 2 seconds on a typical dev machine (best-effort; perf test tracks regressions).

## 1.4 Compatibility Matrix (v1.0)
- [REQ-32] GPU backends via Diligent:
  - Windows: DX12 preferred, Vulkan optional
  - macOS: Metal preferred
  - Linux: Vulkan preferred, OpenGL fallback optional (see Ch3/Ch6)

## 1.5 Requirement → Tests (minimum mapping)
- [TEST-05] Determinism smoke: same inputs => identical serialized sim state hash after N ticks.
- [TEST-06] VFS escape tests: attempts to open `../`, absolute paths, and symlink escape must fail.
- [TEST-07] Binding fuzz/safety: invalid arg types/count must return script error, not crash.
- [TEST-08] Render init: renderer boot + swapchain present for each platform backend configuration.
- [TEST-09] Workbench smoke: launch Workbench, open sample project, run cartridge headless tick for N frames.
- [TEST-10] Log schema: ensure logs include category + severity + cartridge context.


