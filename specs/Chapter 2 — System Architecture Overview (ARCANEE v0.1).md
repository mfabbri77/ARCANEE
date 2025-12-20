# Chapter 2 — System Architecture Overview (ARCANEE v0.1)

## 2.1 Architectural Summary

ARCANEE v0.1 is a cross-platform runtime structured as a **single authoritative main thread** with supporting worker threads for non-script workloads. Cartridge code (Squirrel) executes **only on the main thread** to preserve determinism and simplify debugging.

**Key subsystems**

1. **Platform Layer** (SDL2): windowing, input, high-resolution timing, display enumeration, fullscreen control.
2. **Workbench Layer** (ImGui + TextEditor): editor UI, project navigation, console, run/stop/reload, settings.
3. **Runtime Core**: cartridge lifecycle, Squirrel VM, deterministic scheduling, budgeting enforcement.
4. **Rendering Core** (DiligentEngine): swapchain/backbuffer, render passes, resource lifetime, GPU synchronization.
5. **2D Canvas Engine** (ThorVG): vector rendering into a CPU surface (v0.1), uploaded and composited via GPU.
6. **3D Scene Engine** (Diligent-based layer): glTF/PBR pipeline, camera/lights, post-processing (v0.1 minimal set).
7. **Audio Engine** (SDL audio output + libopenmpt decoding + mixer): music module playback and SFX mixing.
8. **Storage/VFS** (PhysFS): sandbox filesystem for `cart:/`, `save:/`, `temp:/`.
9. **Dev Tools** (Dev Mode): hot reload, debug hooks, optional external debug adapter integration.

------

## 2.2 Threading Model

### 2.2.1 Main Thread (Authoritative)

The main thread owns:

- SDL window and event processing.
- Squirrel VM creation, execution, and destruction.
- All cartridge callbacks: `init()`, `update(dt)`, `draw(alpha)`.
- Diligent rendering context submission (command list recording + queue submission).
- ImGui frame building and rendering.
- Present and swapchain management.

**Constraint:** Cartridge code MUST NOT run on any thread other than the main thread.

### 2.2.2 Worker Threads (Non-script)

ARCANEE v0.1 may use a thread pool (job system) for:

- Asset decoding (image decode, glTF JSON parsing, mesh processing).
- Shader/pipeline compilation (where backend allows background compilation).
- Audio decoding/rendering preparation (libopenmpt rendering into intermediate buffers).
- File I/O via PhysFS (when safe and non-blocking to main thread).

**Constraint:** Worker threads MUST NOT call into the Squirrel VM. Any data transferred to the VM must be marshalled via thread-safe queues and applied on the main thread.

### 2.2.3 Audio Callback Thread

If SDL audio is used in callback mode, audio mixing will occur on a real-time audio thread/callback context.

**Constraints**

- No locks that can block for unbounded time in the audio callback.
- No PhysFS I/O, no GPU work, no Squirrel VM calls from the audio callback.
- Communication from main thread to audio engine must be lock-free or bounded (ring buffer / atomics).

------

## 2.3 Main Loop and Frame Phases

### 2.3.1 High-Level Flow

Each frame proceeds through the following phases in order:

1. **Event Pump**
   - SDL event processing (window resize, input events, quit).
   - Update input state buffers (see Chapter 9).
2. **Workbench UI Update (CPU)**
   - Build ImGui frame.
   - Process editor interactions (open files, edit text, run/stop, settings).
   - Apply requested actions (e.g., start cartridge, reload, toggle scaling mode).
3. **Cartridge Scheduling (CPU)**
   - If cartridge is running and not paused: execute fixed timestep updates:
     - accumulate real time into an accumulator,
     - run `update(dt_fixed)` zero or more times until caught up (bounded by max steps per frame),
     - compute render interpolation `alpha`.
   - Enforce CPU budget and instruction budget (Chapter 4).
4. **Cartridge Draw (CPU → Render Commands)**
   - Call `draw(alpha)` exactly once per video frame when cartridge is running.
   - Cartridge draw records 2D/3D drawing commands into the runtime’s command structures (not immediate GPU calls from script).
5. **Render Execution (GPU)**
   - Build render graph / pass list:
     - 3D pass into CBUF (or into intermediate, then composite to CBUF),
     - 2D canvas composite into CBUF,
     - Present pass CBUF → backbuffer with the active scaling mode,
     - Workbench UI overlay on backbuffer.
   - Submit commands, present swapchain.
6. **Frame End / Diagnostics**
   - Collect timing stats, GPU timestamps (if enabled), update profiler UI.
   - Housekeeping: deferred resource destruction, job completion integration.

### 2.3.2 Fixed Timestep Rules

- The simulation rate is `tick_hz` (default 60).
- `dt_fixed = 1.0 / tick_hz`.
- An accumulator stores unconsumed real time.

**Bounded catch-up**

- The runtime SHALL clamp the number of `update()` calls per frame to `max_updates_per_frame` (default 4) to prevent “spiral of death”.
- If behind beyond this clamp, the runtime SHALL drop excess accumulated time and log a warning in Dev Mode.

**Interpolation**

- `alpha = accumulator / dt_fixed` clamped to `[0,1]`.
- `draw(alpha)` receives alpha to enable interpolation for smoother visuals if cartridge uses it.

#### 2.3.3 Reference Implementation (Normative Pseudocode)

```c++
// Main loop pseudocode - NORMATIVE reference implementation

const double dt_fixed = 1.0 / 60.0;  // 16.67ms
const int max_updates = 4;

double accumulator = 0.0;
double previous_time = getCurrentTimeSeconds();

while (running) {
    double current_time = getCurrentTimeSeconds();
    double frame_time = current_time - previous_time;
    previous_time = current_time;
    
    // Clamp large frame times (e.g., debugger pause)
    if (frame_time > 0.25) frame_time = 0.25;
    
    accumulator += frame_time;
    
    // Phase 1: Event pump
    pumpSDLEvents();
    freezeInputSnapshot();
    
    // Phase 2: Workbench UI
    updateWorkbenchUI();
    
    // Phase 3: Fixed timestep updates (bounded)
    int update_count = 0;
    while (accumulator >= dt_fixed && update_count < max_updates) {
        cartridge.update(dt_fixed);  // Always receives fixed dt
        accumulator -= dt_fixed;
        update_count++;
    }
    
    // Drop excess time if we hit the limit
    if (accumulator > dt_fixed * max_updates) {
        logWarning("Frame budget exceeded, dropping time");
        accumulator = 0.0;
    }
    
    // Phase 4: Cartridge draw with interpolation alpha
    double alpha = accumulator / dt_fixed;
    alpha = clamp(alpha, 0.0, 1.0);
    cartridge.draw(alpha);
    
    // Phase 5: Render execution
    render3DScene();
    composite2DCanvas();
    presentToBackbuffer();
    renderWorkbenchOverlay();
    swapBuffers();
    
    // Phase 6: Diagnostics
    collectFrameTimings();
}
```

------

## 2.4 Subsystem Boundaries and Data Flow

### 2.4.1 Input → Cartridge

Input events are collected by SDL and translated into ARCANEE input state snapshots.

- Cartridge queries input via `inp.*` functions which read from the **current tick’s snapshot**.

### 2.4.2 Cartridge → Render (Command Recording)

Cartridge rendering APIs (`gfx.*`, `gfx3d.*`) do not expose GPU objects directly.

- Calls are translated into internal commands:
  - 2D canvas commands append to a **Canvas Command List**.
  - 3D scene commands modify a **Scene State** and may queue renderable updates.

### 2.4.3 ThorVG → GPU

In v0.1 the 2D vector engine renders into a CPU surface (RGBA/BGRA 32bpp) which is then uploaded to a GPU texture for compositing.

- Upload is performed on main thread during Render Execution phase.
- A staging buffer or dynamic texture update path is used (backend-dependent).

### 2.4.4 PhysFS (VFS) Access

All filesystem access goes through a VFS layer that maps:

- `cart:/` → read-only cartridge mount
- `save:/` → per-cartridge write directory
- `temp:/` → runtime cache

VFS calls may be executed on worker threads if explicitly non-blocking relative to main thread operation. Results are delivered via queues to main thread.

------

## 2.5 Rendering Topology (Overview)

ARCANEE maintains two principal render targets:

1. **Console Framebuffer (CBUF)**: fixed preset resolution chosen by cartridge settings (16:9 / 4:3 presets).
2. **Backbuffer (Swapchain)**: native window/display resolution.

Composition order (v0.1):

1. Render 3D scene → CBUF
2. Render 2D canvas (ThorVG surface uploaded) → composite over CBUF
3. Present pass: scale CBUF → backbuffer using selected present mode:
   - `fit`, `integer_nearest`, `fill`, `stretch`
4. Render Workbench ImGui overlay → backbuffer
5. Present swapchain

Scaling modes are specified formally in Chapter 5.

------

## 2.6 Resource Lifetime and Ownership

ARCANEE resources are owned by the runtime and referenced by cartridges using integer handles:

- ImageHandle, FontHandle, SurfaceHandle
- SceneHandle, MeshHandle, MaterialHandle, TextureHandle
- ModuleHandle, SoundHandle

Rules:

- Handle `0` is invalid.
- Handles may be freed explicitly by the cartridge or reclaimed when the cartridge stops.
- The runtime SHALL prevent use-after-free by validating handles on each API call (Dev Mode: error + log; Player Mode: fail safely).

------

## 2.7 Workbench vs Player Modes

ARCANEE distinguishes operational modes:

### 2.7.1 Workbench Mode

- Editor UI is active.
- Cartridge can be run/paused/reloaded.
- Scaling mode, resolution preset, and aspect can be overridden by the user if permitted.
- Dev diagnostics and profiling UI available.

### 2.7.2 Player Mode (Optional packaging/distribution)

- Editor UI is disabled or minimal.
- Dev Mode features are off by default.
- Overrides may be restricted.

------

## 2.8 Hot Reload and Runtime State (Overview)

Hot reload is initiated from Workbench and may operate in v0.1 as:

- **Full VM reset** (recommended default): destroy and recreate VM, reload scripts, re-run `init()`.
- Optional “soft reload” may be added later, but is not required in v0.1.

Hot reload behavior is specified in Chapter 10.

------

## 2.9 Budgeting and Fault Containment (Overview)

To prevent cartridges from freezing the runtime:

- Each `update()` tick is subject to a CPU time budget (configurable by caps).
- Optionally, the VM may enforce an instruction budget (if implemented).
- The runtime shall detect repeated overruns and may:
  - pause the cartridge,
  - display a Workbench error panel,
  - allow the developer to stop or continue in Dev Mode.

Budget policies are specified in Chapter 12.

------

## 2.10 Required Diagnostics (v0.1)

ARCANEE shall provide (at minimum, in Workbench):

- Console log with timestamps.
- Frame timing: CPU frame time, update time, draw time.
- GPU timing (if supported): pass timings for 3D, 2D upload/composite, present, UI.
- Memory stats: VM memory usage estimate, resource counts, and total VRAM estimate (approximate acceptable).

------

### Chapter 2 Implementation Checklist (Normative)

An implementation is compliant with Chapter 2 if:

1. Cartridge code executes only on the main thread.
2. The main loop implements fixed timestep update with bounded catch-up.
3. Rendering uses the CBUF → Present scaling architecture and overlays Workbench UI in display space.
4. Worker threads are used only for non-VM tasks and communicate results via queues.
5. Audio mixing avoids blocking operations and avoids VM/GPU access in the audio thread.