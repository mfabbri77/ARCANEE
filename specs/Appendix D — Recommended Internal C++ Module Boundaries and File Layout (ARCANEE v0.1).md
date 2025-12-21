# Appendix D — Recommended Internal C++ Module Boundaries and File Layout (ARCANEE v0.1)

## D.1 Scope and Intent

This appendix provides a **recommended internal implementation architecture** for ARCANEE v0.1 in C++. It is normative only where marked; otherwise it is a best-practice guide intended to:

* keep subsystem boundaries clear and testable
* preserve determinism and sandbox security
* support cross-platform backends (DX12/Vulkan/Metal via Diligent)
* support Workbench development tooling (editor, hot reload, debugging, profiling)

The goal is that independent teams can implement ARCANEE consistently from this document.

---

## D.2 Top-Level Repository Layout (Recommended)

A recommended monorepo layout:

```
/arcanee
  /cmake
  /third_party
  /docs
  /tools
  /src
    /app                (program entry, high-level orchestration)
    /platform           (SDL2 windowing/input/timing)
    /vfs                (PhysFS adapter, path normalization)
    /runtime            (cartridge lifecycle, scheduler, budgets)
    /script             (Squirrel VM embed, bindings, module loader)
    /render             (Diligent device, swapchain, render graph, present)
    /render2d           (ThorVG integration, surface management, upload)
    /render3d           (scene system, glTF loader, materials, renderer)
    /audio              (SDL audio device, mixer, libopenmpt integration)
    /workbench          (ImGui, editor, project UI, diagnostics panels)
    /devtools           (profiler, capture, debug hooks, optional adapters)
    /common             (math, utils, logging, asserts, allocators)
    /tests              (unit tests, conformance tests, golden tests)
```

---

## D.3 Core Subsystems and Responsibilities

### D.3.1 `platform` (SDL2)

Responsibilities:

* SDL initialization/shutdown
* Window creation (windowed + fullscreen desktop)
* Drawable pixel size queries (DPI-aware)
* Event pump (keyboard/mouse/gamepad)
* High-resolution monotonic timing

Normative constraints:

* Must not call into Squirrel from other threads.
* Event pump must occur on main thread before update ticks.

Interfaces (recommended):

* `IPlatformWindow`
* `IPlatformInput`
* `IPlatformTime`

### D.3.2 `vfs` (PhysFS)

Responsibilities:

* Mount cartridge directory/archive to `cart:/`
* Map `save:/` and `temp:/` roots per cartridge id
* Path canonicalization (reject `..`, normalize separators)
* Provide synchronous and optionally asynchronous read APIs

Normative constraints:

* Must never expose host filesystem paths to script.
* Must enforce permission checks at call sites (script binding layer) and optionally internally.

Interfaces (recommended):

* `IVfs` with operations mirroring `fs.*`
* `VfsPath` type that stores normalized canonical paths

### D.3.3 `runtime` (Lifecycle + Scheduler + Budgets)

Responsibilities:

* Cartridge state machine (Loading/Running/Paused/Faulted/Stopped)
* Fixed timestep accumulator and tick loop
* Budget enforcement (CPU time per update, watchdog)
* Coordinating subsystem reset on reload/stop

Interfaces (recommended):

* `CartridgeInstance`
* `CartridgeConfig` (manifest + effective policy)
* `BudgetManager`

Normative constraints:

* Cartridge callbacks (`init/update/draw`) must execute on main thread.
* Hot reload v0.1 is full VM reset.

### D.3.4 `script` (Squirrel VM Embed + Bindings)

Responsibilities:

* Create and manage `HSQUIRRELVM`
* Module loader (`require`) with canonical source naming (`cart:/...`)
* Bind `sys/fs/inp/gfx/gfx3d/audio/dev/app` APIs
* Error handler integration (stack traces, file/line mapping)
* Optional debug hooks (Dev Mode)

Recommended separation:

* `ScriptVm` (VM lifetime)
* `ScriptModuleLoader`
* `ScriptBindings_*` (one file per namespace)

Normative constraints:

* No Squirrel VM calls on worker threads.
* All API binding functions must validate args and set last-error.

### D.3.5 `render` (Diligent Device + Swapchain + Render Graph)

Responsibilities:

* Create Diligent device/context and swapchain
* Manage backbuffer resizing, device loss recovery
* Maintain CBUF render targets and depth buffers
* Implement present pipeline (fit/integer/fill/stretch)
* Provide render pass scheduling and resource transitions

Recommended components:

* `RenderDevice` (Diligent setup)
* `SwapchainManager`
* `RenderGraph` or `PassScheduler` (simple v0.1 pass list is fine)
* `PresentPass` (viewport math + sampler selection)

Normative constraints:

* Pass order must match Chapter 5.
* Integer scaling requires point sampling and exact viewport math.

### D.3.6 `render2d` (ThorVG Integration)

Responsibilities:

* Manage ThorVG initialization (software canvas)
* Maintain CPU surfaces (CBUF-size and offscreen surfaces)
* Convert `gfx.*` commands to ThorVG paint objects
* Rasterize per frame and upload to GPU texture(s)
* Enforce 2D limits (path segments, surface pixels, save stack depth)

Recommended components:

* `Canvas2D` (state machine)
* `CanvasSurfacePool` (surfaces, reuse)
* `CanvasUploader` (staging/upload/double-buffering)

Normative constraints:

* Must be deterministic and fail safely on limits.
* Must not block audio thread; should avoid stalls on main thread by buffering uploads.

### D.3.7 `render3d` (Scene + glTF + PBR Renderer)

Responsibilities:

* Scene graph (entities/components)
* glTF loader mapping to internal meshes/materials/textures
* PBR material system (metallic-roughness)
* Camera/light management
* Render 3D pass into CBUF (or an intermediate, then to CBUF)

Recommended components:

* `SceneWorld` (ECS-style or simple component stores)
* `GltfImporter`
* `PbrMaterialSystem`
* `Renderer3D`

Normative constraints:

* Deterministic asset import ordering and handle assignment.
* 3D renders before 2D composition.

### D.3.8 `audio` (SDL Audio + libopenmpt + Mixer)

Responsibilities:

* Audio device init/shutdown, stream callback
* Command queue (main thread → audio thread)
* Module decode/render (libopenmpt) into buffer(s)
* SFX voice mixing, resampling, panning
* Enforce audio limits (voices, memory)

Recommended components:

* `AudioDevice`
* `AudioMixer` (real-time)
* `ModulePlayer` (libopenmpt wrapper)
* `SoundBank` (decoded samples)
* `AudioCommandQueue` (lock-free ring buffer)

Normative constraints:

* No VM, no file I/O, no blocking locks in audio callback.

### D.3.9 `workbench` (ImGui + Editor)

Responsibilities:

* Project selection/open/create
* File tree view (cart:/)
* Text editor windows (ImGuiColorTextEdit)
* Run/Pause/Stop/Reload controls
* Console window, error list, stack trace navigation
* Settings: present mode, preset, vsync, audio volume/latency

Recommended components:

* `WorkbenchApp`
* `EditorPane`
* `ConsolePane`
* `DiagnosticsPane`
* `ProfilerPane`
* `ProjectPane`

Normative constraints:

* Must route input to Workbench or cartridge deterministically (Chapter 9).
* Must show canonical `cart:/` paths and avoid host path leakage in player context.

### D.3.10 `devtools` (Profiling, Capture, Debugging)

Responsibilities:

* CPU/GPU timers aggregation
* Frame capture to `save:/` or `temp:/` only
* Debug hooks integration (optional)
* Optional external debug adapter (Dev Mode only, localhost only by default)

Normative constraints:

* Must not enable external debug ports in Player mode by default.

---

## D.4 Subsystem Interfaces and Ownership Contracts (Recommended)

### D.4.1 Ownership Rules (Normative where marked)

* **Normative:** All cartridge-owned GPU/CPU resources must be released on cartridge stop/reload.
* **Recommended:** Use a `CartridgeResourceArena` that tracks all handles and frees them in bulk.

### D.4.2 Data Flow Contracts

* `script` calls into `runtime`-owned services through narrow interfaces:

  * `IRuntimeServicesSys`
  * `IRuntimeServicesGfx2D`
  * `IRuntimeServicesGfx3D`
  * `IRuntimeServicesAudio`
  * `IRuntimeServicesFs`
* Avoid letting bindings talk directly to low-level Diligent objects.

### D.4.3 Threading Contracts (Normative)

* VM access: main thread only.
* Render submission: main thread only.
* Asset decoding: worker threads allowed, result marshalled.
* Audio mixing: audio thread only; main thread submits commands.

---

## D.5 Build and Dependency Management (Recommended)

### D.5.1 Third-Party Isolation

Place third-party libs under `third_party/` and wrap them behind ARCANEE interfaces:

* SDL2 wrapper in `platform`
* PhysFS wrapper in `vfs`
* ThorVG wrapper in `render2d`
* libopenmpt wrapper in `audio`
* Diligent wrapper in `render`

### D.5.2 Backend Selection

Rendering backend selection should be runtime-configurable:

* `--backend=vulkan|d3d12|metal`
* auto-select default per platform (documented)

---

## D.6 Testing Strategy (Recommended, with Normative Outcomes)

### D.6.1 Unit Tests

* Path normalization and traversal rejection
* Module resolution logic
* Handle validation and use-after-free safety

### D.6.2 Conformance Tests (Normative)

Implement the tests described in Chapter 12 §12.7.
Recommended approach:

* headless mode for logic tests
* offscreen rendering tests with readback for viewport math validation (pixel-perfect patterns)

### D.6.3 Golden Image Tests (Recommended)

For present modes and integer scaling:

* render a known test pattern into CBUF
* present to an offscreen buffer at various W×H
* read back and compare to golden or rule-based expectations

---

## D.7 Recommended Internal File Naming Conventions

* `*_api.h/cpp` for cartridge API binding definitions
* `*_impl.h/cpp` for internal subsystem implementations
* `*_tests.cpp` for tests
* `Arcanee*` prefix for public-facing C++ types

---

## D.8 Minimal “First Milestone” Implementation Plan (Informative)

A practical order to implement ARCANEE v0.1:

1. `platform` + window + timing + basic loop
2. `vfs` mounting + path rules + `fs.*`
3. `script` VM + module loader + `sys.*` + `init/update/draw`
4. `render` swapchain + CBUF + present modes (fit + integer_nearest)
5. `workbench` editor + console + run/reload
6. `render2d` minimal fillRect/text to verify pipeline
7. `inp.*` snapshots + mapping to console space
8. `audio.*` module + SFX (basic)
9. `render3d` glTF + PBR (basic)
10. budgets + watchdog + profiling UI

---

## D.9 Compliance Checklist (Appendix D)

Appendix D is primarily recommended architecture. An implementation “conforms” if:

* It respects the normative threading and ownership constraints identified above.
* It maintains sandbox boundaries and does not leak host paths.
* It can run the reference cartridges and pass the conformance tests.

---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
