<!-- _blueprint/v0.1/ch4.md -->

# Blueprint v0.1 — Chapter 4: Interfaces, APIs, Error Model, ABI

## 4.1 Error Model
- [DEC-21] Use a project-local, header-only error model:
  - `arcanee::Status { Code code; std::string message; }`
  - `arcanee::StatusOr<T>` (or `Expected<T>` equivalent)
  - Rationale: no extra deps; consistent translation across VM/VFS/GPU/audio.
  - Alternatives: exceptions; `tl::expected`; `std::expected` (C++23).
  - Consequences: all subsystem APIs return `Status/StatusOr` for fallible ops.

## 4.2 Thread-Safety Contract (API-level)
- [REQ-39] Unless documented otherwise, subsystem APIs are **not thread-safe** and must be called on the main thread.
- [REQ-40] Audio callback thread must never call into VM/GPU/VFS directly; it consumes lock-free/low-lock buffers only (see Ch6).

## 4.3 Subsystem APIs (minimum contracts)

### 4.3.1 Runtime
- [API-01] `runtime::Runtime`
  - Ownership: app owns Runtime; Runtime owns subsystems.
  - Threading: main thread only.
  - Errors: `Status init(const Config&)`, `Status loadCartridge(path)`, `Status tick()`, `void shutdown()`.
  - Tests: [TEST-05], [TEST-11], [TEST-12].

### 4.3.2 VFS (sandboxed)
- [API-02] `vfs::Vfs`
  - Ownership: Runtime owns Vfs.
  - Threading: main thread; optional read-only background loading via explicit API (vNEXT).
  - API:
    - `Status mountCartridgeRoot(std::string rootPath);`
    - `StatusOr<Bytes> readFile(std::string virtualPath);`
    - `Status writeFile(std::string virtualPath, BytesView);` (writes only into allowed cartridge save namespace)
  - Security invariants:
    - [REQ-22] deny traversal/absolute paths
    - [REQ-22] deny symlink escape (must be tested per platform)
  - Tests: [TEST-06].

### 4.3.3 Script VM
- [API-03] `script::ScriptEngine`
  - Ownership: Runtime owns ScriptEngine; ScriptEngine owns Squirrel VM instance.
  - Threading: main thread only.
  - API:
    - `Status init(const ScriptLimits&);`
    - `Status runEntry(std::string entryFn);`
    - `Status callUpdate(float dt);`
    - `Status hotReload();` (Workbench only)
  - Binding contract:
    - [REQ-24] strict validation; never UB on bad args.
  - Tests: [TEST-07], [TEST-13].

### 4.3.4 Render
- [API-04] `render::RenderDevice`
  - Ownership: Runtime owns RenderDevice; RenderDevice owns Diligent device/context/swapchain.
  - Threading: main thread; GPU sync explicit (Ch6).
  - API:
    - `Status init(const RenderConfig&);`
    - `Status drawFrame(const FrameDesc&);`
    - `Status present();`
  - 2D API: `render::Canvas2D` (retained model as per specs).
  - 3D API: `render::Scene3D` (retained model; PBR + glTF subset).
  - Tests: [TEST-08].

### 4.3.5 Audio
- [API-05] `audio::AudioEngine`
  - Ownership: Runtime owns AudioEngine; AudioEngine owns SDL device and mixer.
  - Threading: command submission on main thread; mixing in audio callback thread.
  - API:
    - `Status init(const AudioConfig&);`
    - `Status playSfx(id, params);`
    - `Status playModule(path);` (optional feature)
  - Tests: [TEST-14] `audio_callback_safety` (no allocations, no locks beyond approved primitives).

### 4.3.6 Workbench
- [API-06] `workbench::Workbench`
  - Ownership: App owns Workbench; Workbench uses Runtime.
  - Threading: UI main thread.
  - API:
    - `Status init();`
    - `void tickUI();`
    - `Status openProject(path);`
  - Tests: [TEST-09], [TEST-13].

## 4.4 Public API Surface (v1.0)
- [DEC-22] v1.0 “public API” is:
  1) Cartridge file formats (manifest, assets)
  2) Script API (Squirrel bindings)
  3) CLI flags (stable-ish)
  - Consequences: no stable C++ ABI promise in v1.0.

## 4.5 ABI & Header Policy
- [REQ-41] Public headers (if introduced) must be declaration-only except templates/inline trivial wrappers.
- [REQ-42] Any ABI-breaking change requires MINOR bump and migration notes (Ch9).

