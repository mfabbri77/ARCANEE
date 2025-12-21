## Chapter 1 — Scope, Goals, Definitions, and Non-Goals

### 1.1 Scope

This document specifies the **ARCANEE v0.1** fantasy console platform, including:

- Cartridge packaging, sandboxing, and virtual filesystem rules.
- Runtime lifecycle and execution model for Squirrel-based cartridges.
- Rendering architecture, including the **Console Framebuffer** concept, scaling/presentation modes (including **integer scaling with nearest filtering**), and editor/workbench composition.
- A formal public API exposed to cartridges (2D Canvas-like API, 3D scene API, audio, input, filesystem, and dev tooling).
- Workbench (integrated editor) requirements and dev-mode features.
- Cross-platform requirements and implementation constraints based on:
  - SDL2 (platform/window/input)
  - DiligentEngine (DX12/Vulkan/Metal rendering)
  - ImGui + ImGuiColorTextEdit (Workbench UI/editor)
  - PhysFS (sandboxed VFS)
  - ThorVG (2D vector canvas backend)
  - libopenmpt (tracker modules audio)

ARCANEE v0.1 is defined as a **complete implementation target**. Any feature labeled “v0.2+” is explicitly out of scope.

------

### 1.2 Product Goals

ARCANEE is designed as a **modern “home computer” experience**: on launch, the user is presented with a **Workbench** where they can immediately edit and run a cartridge project.

Primary goals:

1. **Deterministic cartridge execution** across platforms for a fixed timestep simulation loop.
2. **Sandboxed cartridges**: no direct OS access; all I/O through a controlled virtual filesystem.
3. **Advanced capabilities** (modern 2D vector canvas and modern 3D pipeline) while retaining optional **retro pixel-perfect presentation**.
4. **Cross-platform parity**: Windows, macOS, Linux must behave consistently (API semantics, timing model, scaling rules).
5. **Developer-grade tooling**: integrated editor, console output, hot reload, and debug hooks suitable for step debugging and variable inspection (Dev Mode).

------

### 1.5 Coding and Debugging Standards

### Debugging Code
When inserting temporary logging, assertions, or test code for debugging purposes, you MUST surround the code with the following markers to ensure easy removal:

```cpp
// DEBUG STUFF TO BE REMOVED {start block}
LOG_INFO("Debug value: %d", value);
// DEBUG STUFF TO BE REMOVED {end block}
```

This applies to all source files (C++, Squirrel, CMake). **When the debugging phase is complete, you MUST remove the entire block, including the start/end markers.**

------

### 1.6 Non-Goals (v0.1)

ARCANEE v0.1 explicitly does **not** include:

- Multiplayer/networking API exposed to cartridges (permission stays disabled by default; no shipping net stack requirements).
- User-generated native plugins callable from cartridges (native permission disabled by default).
- Guaranteed binary determinism at floating-point bit level across all CPUs/GPUs (v0.1 targets *logical determinism* and consistent simulation behavior; see §1.9).
- Full typography layout engine (e.g., complex shaping, bi-di, full paragraph layout). v0.1 supports basic text rendering and measurement; advanced layout is deferred.
- A full general-purpose OS shell (file manager, terminals, etc.). Workbench is cartridge-oriented.

------

### 1.4 Target Runtime and Toolchain

- **Host language**: C++17 or later (engine/runtime/tooling).
- **Scripting language**: Squirrel 3.2+ (cartridges).
- **GPU backends**: Vulkan 1.1+ / Metal 2.0+ / Direct3D 12 via DiligentEngine 2.5+.
- **Platform layer**: SDL2 2.28+.
- **Workbench UI**: ImGui 1.89+ with ImGuiColorTextEdit.
- **2D graphics backend**: ThorVG 0.12+.
- **Audio module playback**: libopenmpt 0.7+.
- **Filesystem sandbox**: PhysFS 3.2+.

#### 1.4.1 Build Requirements (Normative)

| Platform | Compiler | Minimum Version |
|----------|----------|-----------------|
| Windows | MSVC | 2019 (v142) |
| Windows | Clang | 14.0 |
| macOS | Clang/Xcode | 14.0 |
| Linux | GCC | 11.0 |
| Linux | Clang | 14.0 |

The implementation MUST compile with C++17 standard (`-std=c++17` or `/std:c++17`).

#### 1.4.2 Color Format Convention (Normative)

All color values in ARCANEE APIs use **32-bit packed ARGB** format:

```
0xAARRGGBB
```

Where:
- `AA` = Alpha channel (0x00 = transparent, 0xFF = opaque)
- `RR` = Red channel
- `GG` = Green channel  
- `BB` = Blue channel

Example: `0xFF00FF00` = fully opaque green.

------

### 1.5 Terminology and Definitions

- **Workbench**: The integrated development environment that opens at launch (editor + file browser + console + run/debug controls).
- **Cartridge**: A packaged project containing scripts and assets (read-only at runtime except through save storage).
- **Runtime**: The execution environment that runs cartridges (VM, scheduling, GPU submission, audio engine, etc.).
- **VM**: The Squirrel Virtual Machine instance executing cartridge code.
- **Console Framebuffer (CBUF)**: The internal render target that represents the cartridge’s “screen” at a fixed preset resolution and aspect ratio.
- **Display Backbuffer**: The swapchain backbuffer for the OS window/display.
- **Present Mode**: The scaling/viewport algorithm that maps CBUF to the backbuffer (e.g., `fit`, `integer_nearest`, `fill`, `stretch`).
- **Dev Mode**: A privileged runtime mode intended for development (enables debugging, hot reload, internal profiling, and optional external debug adapters). Not enabled for “player” distribution by default.

------

### 1.6 Determinism Model (v0.1)

ARCANEE v0.1 requires the following determinism guarantees:

1. **Fixed timestep update**: Cartridge `update(dt)` is executed with a constant `dt = 1 / tick_hz`.
2. **Frame-stable input sampling**: Input queried during a given update tick is stable and consistent.
3. **Deterministic RNG**: The runtime provides a deterministic RNG stream per cartridge unless the cartridge explicitly requests nondeterministic time sources.
4. **No cartridge-level multithreading**: Cartridge code runs on a single thread to avoid race conditions and nondeterministic scheduling.

#### 1.6.1 Default Timing Values (Normative)

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `tick_hz` | 60 | Simulation ticks per second |
| `dt_fixed` | 1/60 ≈ 0.01667s | Fixed delta time passed to `update(dt)` |
| `max_updates_per_frame` | 4 | Maximum catch-up ticks per frame |

#### 1.6.2 RNG Algorithm (Normative)

The runtime MUST implement `sys.rand()` using **xorshift128+** algorithm for deterministic, portable random number generation.

```c++
// Normative reference implementation
struct Xorshift128Plus {
    uint64_t state[2];
    
    uint64_t next() {
        uint64_t s1 = state[0];
        uint64_t s0 = state[1];
        state[0] = s0;
        s1 ^= s1 << 23;
        state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
        return state[1] + s0;
    }
};
```

`sys.rand()` MUST return an integer in range `[0, 2^31 - 1]` derived from the upper bits of the xorshift128+ output.

`sys.srand(seed)` MUST initialize both state words deterministically from the seed.

Non-guarantees (v0.1):

- Exact floating-point bitwise determinism across different CPU architectures is not guaranteed. The platform targets practical gameplay determinism and consistent results within typical tolerance.

------

### 1.7 Security and Sandboxing Requirements

Cartridges must be treated as untrusted code. The runtime shall:

- Prevent access to the host OS filesystem except through the sandboxed VFS.
- Prevent network access unless explicitly permitted (permission default OFF).
- Enforce execution budgets and limits to prevent hangs/freezes:
  - CPU time budget per `update()` tick (configurable; default defined by system policy).
  - VM memory limits and/or object allocation limits.
- Ensure that exposing “developer features” (debug adapters, sockets) is restricted to Dev Mode and local-only by default.

------

### 1.8 Cross-Platform Presentation Requirements

ARCANEE distinguishes two rendering spaces:

1. **Workbench/UI Space**: Native display/window resolution (windowed size or fullscreen desktop resolution).
2. **Cartridge/Console Space**: CBUF preset resolutions (16:9 and 4:3).

In fullscreen for the Workbench:

- The runtime shall use **desktop fullscreen** (borderless at native desktop resolution) rather than forcing a mode switch.

------

### 1.9 Versioning and Compatibility

#### 1.9.1 Version String Format (Normative)

All version strings MUST follow [Semantic Versioning 2.0](https://semver.org/) format:

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

Examples:
- `0.1.0` — ARCANEE v0.1 release
- `0.1.0-alpha.1` — Pre-release
- `1.0.0+20231215` — Release with build metadata

#### 1.9.2 API Functions

| Function | Returns | Example |
|----------|---------|--------|
| `sys.getApiVersion()` | API version string | `"0.1.0"` |
| `sys.getEngineVersion()` | Engine version string | `"0.1.0-dev"` |

#### 1.9.3 Manifest Version Fields

| Field | Required | Description |
|-------|----------|-------------|
| `api_version` | YES | Target API version (e.g., `"0.1"`) |
| `version` | YES | Cartridge version (semver) |
| `engine_min` | NO | Minimum engine version required |

The runtime MUST reject cartridges where `api_version` indicates a newer major or minor version than supported.

------

### 1.10 Document Structure

The remainder of this specification is organized into chapters intended to be read and implemented sequentially:

- Chapter 2: System Architecture Overview (threads, main loop, subsystems)
- Chapter 3: Cartridge Format, VFS, Permissions, and Sandbox
- Chapter 4: Execution Model and Squirrel Integration
- Chapter 5: Rendering Architecture (CBUF, scaling, present modes, formats)
- Chapter 6: 2D Canvas API (ThorVG-backed)
- Chapter 7: 3D Scene API (Diligent-backed)
- Chapter 8: Audio System and API (libopenmpt + SFX)
- Chapter 9: Input System and API
- Chapter 10: Workbench UX, Hot Reload, Debugging, and Profiling
- Chapter 11: Error Handling, Logging, and Diagnostics
- Chapter 12: Performance Budgets, Limits, and Compliance Tests

------


---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
