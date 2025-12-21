# Appendix H — Specification Clarifications and Developer Disambiguation (ARCANEE v0.1)

## H.1 Scope and Purpose

This appendix provides **developer-oriented clarifications** for areas of the ARCANEE v0.1 specification that require disambiguation during implementation. These clarifications resolve ambiguities, provide additional context, and specify precise behaviors for edge cases not fully covered in the main chapters.

---

## H.2 Cartridge and Manifest Clarifications

### H.2.1 Manifest Format Priority

**Question**: When both `cartridge.toml` and `cartridge.json` exist, which takes precedence?

**Answer (Normative)**: 
- The runtime MUST check for `cartridge.toml` first.
- If `cartridge.toml` does not exist, check for `cartridge.json`.
- If both exist, `cartridge.toml` takes precedence.
- If neither exists, reject the cartridge with error "Missing manifest file".

### H.2.2 Cartridge ID Format

**Question**: What are the valid formats for the cartridge `id` field?

**Answer (Normative)**:
- The `id` MUST be a non-empty UTF-8 string.
- RECOMMENDED format: reverse-domain notation (e.g., `com.example.mygame`).
- The `id` MUST contain only: lowercase letters `[a-z]`, digits `[0-9]`, periods `.`, hyphens `-`, underscores `_`.
- Maximum length: 255 characters.
- The `id` is case-sensitive for mapping save directories.

### H.2.3 Entry Script Default

**Question**: If `entry` is omitted from the manifest, what happens?

**Answer (Normative)**:
- If `entry` is omitted, the runtime MUST default to `"main.nut"`.
- If `main.nut` does not exist and `entry` is omitted, the cartridge MUST be rejected.

### H.2.4 Manifest Unknown Fields

**Question**: How should the runtime handle unknown fields in the manifest?

**Answer (Normative)**:
- Unknown top-level fields MUST be ignored.
- Unknown nested fields within known sections (e.g., `[display]`, `[permissions]`) MUST be ignored.
- This allows forward compatibility with future manifest extensions.

---

## H.3 VFS and Path Handling Clarifications

### H.3.1 Case Sensitivity

**Question**: Is the VFS case-sensitive?

**Answer (Normative)**:
- The VFS MUST treat paths as **case-sensitive** on all platforms.
- `cart:/Foo.png` and `cart:/foo.png` are distinct paths.
- This ensures deterministic behavior and cross-platform compatibility.
- Cartridge authors should avoid relying on case-insensitive file systems.

### H.3.2 Trailing Slashes

**Question**: How should trailing slashes in paths be handled?

**Answer (Normative)**:
- Trailing slashes MUST be **normalized away** (removed).
- `cart:/assets/` becomes `cart:/assets`.
- `cart:/assets//file.png` becomes `cart:/assets/file.png`.

### H.3.3 Empty Path Components

**Question**: What happens with paths like `cart:///file.png` or `cart:/./file.png`?

**Answer (Normative)**:
- Empty components (from `//`) MUST be collapsed.
- `.` components MUST be resolved and removed.
- `cart:///foo/./bar//baz` becomes `cart:/foo/bar/baz`.

### H.3.4 Maximum Path Length

**Question**: What is the maximum allowed path length?

**Answer (Normative)**:
- Maximum normalized VFS path length: **240 characters** (excluding namespace prefix).
- Paths exceeding this limit MUST be rejected with `sys.getLastError()` set appropriately.

### H.3.5 Allowed Filename Characters

**Question**: What characters are allowed in filenames?

**Answer (Normative)**:
- Allowed: alphanumerics `[A-Za-z0-9]`, underscore `_`, hyphen `-`, period `.`.
- Allowed (but discouraged): spaces, other Unicode characters.
- **Forbidden**: `/ \ : * ? " < > |` (control characters and filesystem-unsafe chars).
- Paths containing forbidden characters MUST be rejected.

---

## H.4 Squirrel Execution Clarifications

### H.4.1 Entry Point Detection

**Question**: How should the runtime detect missing `init`, `update`, or `draw` functions?

**Answer (Normative)**:
- After executing the entry script (and all `require`d modules), the runtime MUST verify:
  1. Global `init` exists and is a function/closure.
  2. Global `update` exists and is a function/closure.
  3. Global `draw` exists and is a function/closure.
- If any are missing or not callable, enter **Faulted** state with message "Missing required entry point: <name>".

### H.4.2 Module Circular Dependencies

**Question**: How should the runtime handle circular `require()` dependencies?

**Answer (Normative)**:
- Circular dependencies MUST be detected at the module loader level.
- When module A requires module B, and module B requires module A:
  - If A is currently being loaded (not yet cached), return the **partially constructed** module table (like Node.js behavior).
  - This allows mutual references but may result in `null` values for not-yet-defined exports.
- The runtime MUST NOT enter an infinite loop.
- A deprecation warning MAY be logged in Dev Mode.

### H.4.3 Relative vs Absolute `require()` Paths

**Question**: How are `require()` paths resolved?

**Answer (Normative)**:
```
require("foo")           → relative to current module directory
require("foo.nut")       → relative to current module directory  
require("./foo")         → explicit relative to current module directory
require("../foo")        → REJECTED (.. not allowed)
require("cart:/foo")     → absolute VFS path
require("save:/data")    → REJECTED (only cart:/ allowed for require)
```

### H.4.4 Module Return Value

**Question**: What does `require()` return?

**Answer (Normative)**:
- If the module script uses `return value;`, `require()` returns that value.
- If no explicit return, `require()` returns `null`.
- Modules typically return a table of exports.

---

## H.5 Rendering Clarifications

### H.5.1 CBUF Clear Behavior

**Question**: Is the CBUF cleared automatically at the start of `draw()`?

**Answer (Normative)**:
- The CBUF is **NOT automatically cleared** between frames.
- Cartridges MUST explicitly call `gfx.clear(color)` if they want to clear.
- This allows techniques like motion blur trails or persistent backgrounds.

### H.5.2 Draw Order: 3D vs 2D

**Question**: Does `gfx3d.render()` have to be called before 2D drawing?

**Answer (Normative)**:
- The runtime renders in a fixed pass order: 3D → 2D → Present.
- `gfx3d.render()` records the 3D scene to be rendered into CBUF.
- All `gfx.*` 2D commands are composited **on top of** the 3D result.
- The cartridge does not control pass order; it can only control whether 3D is rendered (by calling `gfx3d.render()`).

### H.5.3 Multiple `gfx3d.render()` Calls

**Question**: What happens if `gfx3d.render()` is called multiple times in one frame?

**Answer (Normative)**:
- Each call to `gfx3d.render()` re-renders the 3D scene.
- Only the **last** render output is used in the composition pipeline.
- Dev Mode SHOULD warn if multiple calls are detected (performance issue).

### H.5.4 Surface Size Limits

**Question**: What is the maximum surface size?

**Answer (Normative)**:
- Maximum single surface: 4096 × 4096 pixels (16,777,216 total).
- Maximum total surface pixels (sum of all surfaces): 33,554,432 (as per caps).
- Exceeding limits causes `gfx.createSurface()` to return `0` and set last error.

### H.5.5 Drawing Outside Target Bounds

**Question**: What happens when drawing commands extend outside the render target?

**Answer (Normative)**:
- Drawing is **clipped to target bounds** automatically.
- No error is raised for out-of-bounds drawing.
- Coordinates outside bounds are valid input (geometry is clipped).

---

## H.6 Input Clarifications

### H.6.1 Mouse Position During Pause

**Question**: What does `inp.mousePos()` return when the cartridge is paused?

**Answer (Normative)**:
- Input snapshots continue to update even when paused.
- `inp.mousePos()` returns the current console-space mouse position.
- This allows debugging overlays to respond to mouse input.

### H.6.2 Key State After Window Focus Loss

**Question**: What happens to key states when the window loses focus?

**Answer (Normative)**:
- All input states are **reset to released** on focus loss.
- The next tick after focus loss will have all keys/buttons as `Down=false`.
- `Released` edges will trigger for any keys that were held.
- On focus regain, state starts fresh (no phantom key presses).

### H.6.3 Multiple Gamepads Same Model

**Question**: If two identical gamepads are connected, how are they indexed?

**Answer (Normative)**:
- Gamepads are indexed by connection order: first connected = `pad 0`.
- The index is **stable within a session** (until disconnect).
- If pad 0 disconnects, the slot becomes invalid; pad 1 does not shift to 0.
- Disconnected pads return default values (buttons=false, axes=0).

### H.6.4 Touch Input

**Question**: Is touch input supported in v0.1?

**Answer (Normative)**:
- Touch input is **not required** in v0.1.
- Touch may be mapped to mouse input by the platform layer (SDL).
- A dedicated touch API is deferred to v0.2+.

---

## H.7 Audio Clarifications

### H.7.1 Multiple Module Playback

**Question**: Can multiple music modules play simultaneously?

**Answer (Normative)**:
- **No**. Only one music module may be active at a time.
- Calling `audio.playModule(m)` while another is playing MUST stop the previous module.
- Optionally, a brief crossfade may be implemented (v0.1 does not require it).

### H.7.2 Voice Stealing Policy

**Question**: When all SFX voices are in use, which voice is stolen?

**Answer (Normative)**:
- Default stealing policy: **oldest voice** (first started is first stolen).
- Alternative implementations MAY use "quietest" or "oldest quietest" but MUST document the policy.
- The policy MUST be deterministic and consistent.

### H.7.3 Sound Format Requirements

**Question**: Which audio formats MUST be supported?

**Answer (Normative)**:
- **Required**: WAV (PCM 8-bit, 16-bit, 32-bit float; mono/stereo).
- **Recommended**: OGG Vorbis.
- **Module formats**: All formats supported by libopenmpt (documented per build).

### H.7.4 Sample Rate Mismatch

**Question**: How are sample rate differences handled?

**Answer (Normative)**:
- Sounds loaded at different sample rates than the device rate are resampled.
- Resampling MUST be performed using a deterministic algorithm (linear is acceptable for v0.1).
- The runtime SHOULD document the resampling method.

---

## H.8 API Return Value Clarifications

### H.8.1 Handle Value `0`

**Question**: Is `0` ever a valid handle?

**Answer (Normative)**:
- **No**. Handle value `0` is reserved as the "invalid/null" handle for all handle types.
- Valid handles start at `1`.

### H.8.2 Boolean vs Integer Returns

**Question**: Do boolean-returning functions use Squirrel's `true/false` or integers?

**Answer (Normative)**:
- Boolean-returning functions MUST return Squirrel `true` or `false`.
- NOT 1 or 0 as integers.
- This ensures type consistency in Squirrel scripts (`if (result)` vs `if (result == true)`).

### H.8.3 `null` vs Missing Return

**Question**: For table-returning functions, is `null` returned or nothing?

**Answer (Normative)**:
- Failed table-returning functions MUST return explicit `null`.
- They MUST NOT return nothing (which causes Squirrel runtime issues).

---

## H.9 Error Handling Clarifications

### H.9.1 Last Error Persistence

**Question**: Is `sys.getLastError()` cleared after a successful API call?

**Answer (Normative)**:
- **No**. `sys.getLastError()` retains its value until:
  - Another error occurs (overwrites it), OR
  - `sys.clearLastError()` is explicitly called.
- Successful calls do NOT clear the last error.

### H.9.2 Error Message Format

**Question**: What format should error messages follow?

**Answer (Normative)**:
- Format: `"<function>: <description>"`
- Example: `"gfx.loadImage: file not found: cart:/missing.png"`
- Messages MUST be human-readable and actionable.
- In Player Mode, host-specific paths MUST be omitted.

---

## H.10 Workbench Clarifications

### H.10.1 Editor Tabs Limit

**Question**: Is there a limit on open editor tabs?

**Answer (Normative)**:
- No hard limit is mandated.
- Implementations SHOULD limit to a reasonable number (e.g., 50) for UI performance.
- Workbench MUST gracefully handle attempting to open more.

### H.10.2 Unsaved Changes on Reload

**Question**: What happens to unsaved editor changes on hot reload?

**Answer (Normative)**:
- Hot reload uses the **on-disk** version of files.
- Unsaved changes are NOT included in the reload.
- Workbench SHOULD warn the user that unsaved changes exist before reload.

### H.10.3 Breakpoint Persistence

**Question**: Are breakpoints saved across sessions?

**Answer (Normative)**:
- Workbench SHOULD persist breakpoints per-project (in workspace metadata).
- If the file is modified or removed, dangling breakpoints are ignored/cleared.

---

## H.11 Timing and Determinism Clarifications

### H.11.1 `sys.timeMs()` Behavior

**Question**: What is the epoch for `sys.timeMs()`?

**Answer (Normative)**:
- `sys.timeMs()` returns **milliseconds since cartridge start** (not system epoch).
- This provides a reasonable range without overflow concerns.
- For absolute time, cartridges should not rely on this value.

### H.11.2 Accumulator Reset on Pause

**Question**: What happens to the fixed-timestep accumulator when paused?

**Answer (Normative)**:
- On pause, the accumulator is **frozen** (no time accumulates).
- On resume, the accumulator continues from its frozen value.
- Wall-clock time during pause is discarded.

### H.11.3 Hot Reload Timing

**Question**: Does hot reload affect the accumulated time?

**Answer (Normative)**:
- Full VM reset (hot reload) resets:
  - Frame counter to 0
  - RNG seed (if not explicitly re-seeded by cartridge)
  - Accumulator to 0
- `sys.timeMs()` MAY or MAY NOT reset depending on implementation (SHOULD reset for clean starts).

---

## H.12 Resource Management Clarifications

### H.12.1 Double-Free Safety

**Question**: What happens if a resource is freed twice?

**Answer (Normative)**:
- Freeing an already-freed handle MUST be a no-op.
- It MUST NOT crash or corrupt state.
- Dev Mode MAY log a warning about double-free.

### H.12.2 Resource Limits per Cartridge

**Question**: Are resource limits per-frame or lifetime?

**Answer (Normative)**:
- Limits like `max_surfaces = 32` are **concurrent** (alive at the same time).
- A cartridge can create, free, and create again without counting toward the limit.
- Only currently-held resources count toward limits.

### H.12.3 Resource Cleanup on Fault

**Question**: Are resources freed when a cartridge enters Faulted state?

**Answer (Normative)**:
- Resources remain allocated until explicitly freed or the cartridge is Stopped.
- This allows debugging/inspection of resource state after a fault.
- Stopping the cartridge MUST free all resources.

---

## H.13 Platform-Specific Clarifications

### H.13.1 Fullscreen Modes

**Question**: What fullscreen implementations are acceptable?

**Answer (Normative)**:
- **Required**: Desktop fullscreen (borderless windowed at desktop resolution).
- **Optional (v0.1)**: Exclusive fullscreen with mode switching.
- Workbench MUST default to desktop fullscreen.
- Exclusive mode, if implemented, MUST be opt-in.

### H.13.2 High-DPI Handling

**Question**: How is high-DPI/Retina handled?

**Answer (Normative)**:
- CBUF dimensions are **logical pixels** (not affected by DPI scaling).
- Backbuffer dimensions are **physical pixels** (DPI-scaled).
- Present pass scales from CBUF to physical pixels.
- CBUF does NOT change with display DPI.

### H.13.3 Multi-GPU Systems

**Question**: How should multi-GPU systems be handled?

**Answer (Normative)**:
- v0.1 does not require explicit multi-GPU support.
- The runtime MAY use driver defaults.
- Explicit GPU selection is a v0.2+ feature.

---

## H.14 Compliance Checklist (Appendix H)

This appendix provides clarifications, not new requirements. An implementation that satisfies the main chapters and appendices A-G should already handle the cases described here. This appendix serves as:

1. A developer reference for edge case behaviors.
2. A source of test cases for conformance validation.
3. A disambiguation guide for specification reading.

---

*End of Appendix H*

---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
