# Chapter 4 — Execution Model and Squirrel Integration (ARCANEE v0.1)

## 4.1 Overview

This chapter specifies the normative execution model for cartridges and the embedding rules for the Squirrel VM in ARCANEE v0.1. It defines:

* VM lifecycle and cartridge lifecycle states
* Script compilation/loading rules and module system behavior
* Binding model for the public ARCANEE API (`sys.*`, `fs.*`, `inp.*`, `gfx.*`, `gfx3d.*`, `audio.*`, `dev.*`)
* Determinism constraints and time semantics (`init/update/draw`)
* Budget enforcement (time and optional instruction budgeting)
* Error handling, reporting, and debug metadata requirements
* Coroutine policy (optional but specified for forward compatibility)

---

## 4.2 Cartridge Lifecycle and State Machine

### 4.2.1 States

The runtime SHALL manage each cartridge instance using the following states:

1. **Unloaded**: No VM instance exists.
2. **Loading**: VFS mounted; VM creation in progress; scripts compiling/loading.
3. **Initialized**: `init()` executed successfully.
4. **Running**: Fixed-step `update(dt_fixed)` and per-frame `draw(alpha)` executed.
5. **Paused**: Cartridge execution is suspended (no `update()`; `draw()` MAY continue depending on Workbench policy).
6. **Faulted**: Cartridge encountered an unrecoverable error; execution halted.
7. **Stopped**: Cartridge intentionally stopped by user; VM may remain for inspection or be destroyed.

### 4.2.2 Legal Transitions

* Unloaded → Loading → Initialized → Running
* Running ↔ Paused
* Running → Faulted
* Running/Paused/Faulted → Stopped → Unloaded
* Stopped → Loading (restart)

Transitions MUST be initiated only by the runtime (Workbench commands are requests).

---

## 4.3 VM Instance Model

### 4.3.1 VM Ownership

* Each running cartridge SHALL have exactly **one** Squirrel VM instance (HSQUIRRELVM).
* The VM SHALL be created and destroyed on the **main thread**.
* Cartridge code MUST execute only on the main thread.

### 4.3.2 VM Initialization Requirements

On VM creation, the runtime MUST:

1. Create the VM with a configured initial stack size (implementation-defined but consistent).
2. Register an error handler and print/log callbacks.
3. Install the ARCANEE global environment and API tables.
4. Configure the module loading subsystem (see §4.4).
5. Apply sandbox restrictions (see §4.6).
6. Configure debug metadata generation policy (see §4.8).

### 4.3.3 VM Destruction Requirements

On VM destruction, the runtime MUST:

* Release all VM-owned objects and closures.
* Release or invalidate all runtime handles exposed to the VM.
* Flush pending VM-related tasks/queues.
* Invalidate any cached references to VM objects in host code.

The runtime MUST ensure that no audio thread or worker thread retains or dereferences VM objects after destruction.

---

## 4.4 Script Loading and Module System

### 4.4.1 Entry Script Loading

The cartridge entry script path is defined by the manifest field `entry`. The runtime SHALL load the entry script as follows:

1. Read bytes from `cart:/<entry>`.
2. Compile using a **source name** equal to its canonical VFS path (e.g., `cart:/main.nut`).
3. Execute the compiled closure in the cartridge root environment.

If compilation or execution fails, the cartridge SHALL enter **Faulted** state.

### 4.4.2 Canonical Source Names (Normative)

All compiled scripts MUST have a canonical source name used for:

* Error messages (file/line attribution)
* Breakpoints and stepping (Dev Mode)
* Hot reload diagnostics

The source name MUST be the normalized VFS path using `/` separators and MUST include the namespace prefix:

* Example: `cart:/src/player.nut`

### 4.4.3 Module Loading (`require`)

ARCANEE v0.1 SHALL support a module load function analogous to `require(path)`.

#### Resolution rules (normative)

* `require("cart:/src/foo.nut")` loads exactly that VFS path.
* `require("src/foo.nut")` is resolved relative to the requiring module’s directory under `cart:/`.

  * Example: requiring from `cart:/src/main.nut`, `require("util.nut")` resolves to `cart:/src/util.nut`.
* Implicit extension resolution MAY be supported (`"foo"` → `"foo.nut"`), but if implemented it MUST be deterministic and documented.

#### Caching rules (normative)

* Modules SHALL be cached by canonical VFS path.
* Requiring the same module path multiple times MUST return the same module value (idempotent load).

#### Execution model (normative)

* A module is compiled and executed once.
* The module’s return value (or exports table) is stored in the module cache.

#### Failure behavior

* Any compilation/execution error in a required module MUST propagate to the caller and fault the cartridge unless caught by script.

### 4.4.4 Standard Library Import

ARCANEE provides a “standard library” under `cart:/std/` (or an internal equivalent).

* The runtime SHOULD preload `std` into the global environment for convenience, or provide `require("std")`.
* Any preloaded library MUST remain deterministic and sandbox-safe.

---

## 4.5 Cartridge Callback Semantics (`init`, `update`, `draw`)

### 4.5.1 Mandatory Functions

Cartridges MUST define the following global functions:

* `function init() { ... }`
* `function update(dt) { ... }`
* `function draw(alpha) { ... }`

If any are missing, the runtime MUST refuse to run and report an error.

### 4.5.2 Call Ordering (Normative)

* `init()` is called exactly once after the entry script executes successfully and before any `update/draw`.
* `update(dt_fixed)` is called zero or more times per host frame as described in Chapter 2.
* `draw(alpha)` is called at most once per host frame when the cartridge is Running. In Paused state, `draw()` MAY still be called to allow debugging overlays; this is a Workbench policy and MUST be documented.

### 4.5.3 Time Arguments

* `dt` passed to `update()` MUST be constant and equal to `1.0 / tick_hz`.
* `alpha` passed to `draw()` MUST be within `[0,1]` and reflect interpolation fraction.

### 4.5.4 Side Effects and Determinism

Cartridge logic intended to be deterministic SHOULD:

* Use `sys.rand()` for deterministic randomness.
* Avoid `sys.timeMs()` for simulation decisions (allowed for UI/metrics only).

---

## 4.6 Sandboxing and Exposed VM Features

### 4.6.1 Global Environment

The runtime SHALL provide a controlled global root table (the “cartridge global environment”). The environment MUST include:

* ARCANEE API tables: `sys`, `fs`, `inp`, `gfx`, `gfx3d`, `audio`, and optionally `dev` (Dev Mode only)
* A deterministic `std` library (if enabled)

The runtime MUST ensure that:

* The cartridge cannot access host OS APIs.
* The cartridge cannot load native libraries or arbitrary files outside VFS.
* The cartridge cannot mutate critical runtime tables in ways that compromise sandbox guarantees.

### 4.6.2 Standard Squirrel Libraries

ARCANEE v0.1 MUST define a policy for Squirrel standard libraries:

* **Base language features** (tables, arrays, classes, closures) are available.
* Potentially unsafe or non-deterministic libraries (I/O, OS, sockets) MUST NOT be exposed unless explicitly sandboxed and permission-checked.

The runtime MAY provide selected libraries if they can be proven sandbox-safe (e.g., pure string/regex utilities), but this MUST be consistent across platforms.

### 4.6.3 Resource Handles and Safety

All native resources exposed to Squirrel MUST be represented as integer handles.

* Handle validation MUST occur on every API call.
* Use-after-free MUST fail safely.

---

## 4.7 API Binding Model (Host ↔ Squirrel)

### 4.7.1 Naming and Namespaces (Normative)

ARCANEE public API MUST be exposed under fixed namespaces:

* `sys.*` — runtime and display control
* `fs.*` — sandbox VFS
* `inp.*` — input state
* `gfx.*` — 2D canvas API
* `gfx3d.*` — 3D scene API
* `audio.*` — audio module playback and SFX
* `dev.*` — Dev Mode only (hot reload hooks, profiling markers)

No top-level functions (global) SHOULD be introduced beyond `init/update/draw` to avoid name collisions.

### 4.7.2 Argument Checking (Normative)

Every bound native function MUST:

* Validate arity (argument count).
* Validate type of each argument.
* Validate handle ownership and state (e.g., a surface handle is valid and belongs to this cartridge instance).
* On failure, return `false/null` and set `sys.getLastError()` (or throw only in Dev Mode if configured).

### 4.7.3 Allocation and Performance Rules

Native bindings MUST avoid:

* Unbounded allocations per call.
* Returning large data structures every frame unless necessary.

Where large results are needed (e.g., directory listing), the runtime SHOULD:

* Allocate once per call and return an array
* Enforce quotas and maximum lengths

### 4.7.4 Reference Implementation (Normative Example)

```c++
// Squirrel binding example - NORMATIVE pattern for all API functions

// Example: gfx.drawImage(img, dx, dy)
SQInteger sq_gfx_drawImage(HSQUIRRELVM vm) {
    // 1. Validate arity
    SQInteger nargs = sq_gettop(vm);
    if (nargs != 4) {  // this + 3 args
        setLastError("gfx.drawImage: expected 3 arguments (img, x, y)");
        sq_pushbool(vm, SQFalse);
        return 1;
    }
    
    // 2. Validate argument types
    SQInteger imgHandle;
    SQFloat dx, dy;
    
    if (SQ_FAILED(sq_getinteger(vm, 2, &imgHandle))) {
        setLastError("gfx.drawImage: arg 1 must be integer (ImageHandle)");
        sq_pushbool(vm, SQFalse);
        return 1;
    }
    if (SQ_FAILED(sq_getfloat(vm, 3, &dx)) ||
        SQ_FAILED(sq_getfloat(vm, 4, &dy))) {
        setLastError("gfx.drawImage: args 2,3 must be numbers");
        sq_pushbool(vm, SQFalse);
        return 1;
    }
    
    // 3. Validate handle (exists, correct type, owned by this cartridge)
    Image* img = g_runtime->getImage(static_cast<Handle>(imgHandle));
    if (!img) {
        setLastError("gfx.drawImage: invalid ImageHandle");
        sq_pushbool(vm, SQFalse);
        return 1;
    }
    if (img->owner != g_currentCartridge) {
        setLastError("gfx.drawImage: handle not owned by this cartridge");
        sq_pushbool(vm, SQFalse);
        return 1;
    }
    
    // 4. Execute operation
    g_canvas->drawImage(img, dx, dy);
    
    sq_pushbool(vm, SQTrue);
    return 1;
}

// Registration pattern
void registerGfxModule(HSQUIRRELVM vm) {
    sq_pushstring(vm, "gfx", -1);
    sq_newtable(vm);
    
    sq_pushstring(vm, "drawImage", -1);
    sq_newclosure(vm, sq_gfx_drawImage, 0);
    sq_newslot(vm, -3, SQFalse);
    
    // ... more functions ...
    
    sq_newslot(vm, -3, SQFalse);  // Add gfx table to root
}
```

---

## 4.8 Debug Metadata and Developer Instrumentation

### 4.8.1 Debug Info Compilation Policy

ARCANEE v0.1 MUST support two compilation profiles:

1. **Dev Compilation** (Workbench / Dev Mode): debug metadata enabled (line info and symbol info where supported).
2. **Player Compilation** (distribution): debug metadata MAY be disabled for performance/size.

The compilation profile MUST be selectable per cartridge run configuration in Workbench.

### 4.8.2 Source Mapping Requirements

Errors MUST report:

* Canonical VFS source name (e.g., `cart:/src/player.nut`)
* Line number
* If available, function name

Workbench MUST be able to navigate to the corresponding file/line in the editor.

### 4.8.3 Hook Integration (Dev Mode)

In Dev Mode, the runtime MAY install a debug hook to support:

* Breakpoints
* Step over/into/out
* Stack inspection
* Local variable inspection

If installed, the hook MUST:

* Be disabled by default in Player Mode.
* Be bounded in overhead and avoid unbounded per-line work unless debugging is actively attached.

---

## 4.9 Budget Enforcement (Normative)

### 4.9.1 CPU Time Budget per Update Tick

ARCANEE MUST enforce a maximum CPU time per `update(dt_fixed)` call, as specified by policy (`caps.cpu_ms_per_update` clamped by runtime). Enforcement requirements:

* Measure elapsed time for each `update()` invocation on the main thread.
* If the budget is exceeded:

  * In Workbench: flag an overrun, log diagnostics, optionally pause the cartridge after repeated overruns.
  * In Player Mode: apply the configured policy (pause, stop, or continue with throttling), but MUST avoid freezing the entire runtime.

### 4.9.2 Optional Instruction Budget

If the Squirrel embedding allows instruction counting or debug-hook-based instruction limiting, ARCANEE MAY enforce an instruction budget per tick. If implemented:

* The instruction limit MUST be deterministic.
* The failure mode MUST match time budget policy (error + pause/stop per configuration).

### 4.9.3 Infinite Loop and Hang Protection

ARCANEE MUST provide hang protection in Workbench:

* If `update()` fails to return within a configurable watchdog threshold, the runtime MUST interrupt execution (if feasible) or terminate the cartridge VM and enter Faulted state.

---

## 4.10 Coroutine Policy (v0.1)

### 4.10.1 Availability

ARCANEE v0.1 MAY allow Squirrel coroutines (threads) for cooperative scheduling inside cartridges.

### 4.10.2 Determinism Rules

If coroutines are enabled:

* Only cooperative yields are permitted (no OS threads).
* The runtime MUST ensure that coroutine scheduling is deterministic:

  * A recommended approach is to provide a runtime scheduler in `std.time` (e.g., `waitFrames(n)`) that yields and resumes in a deterministic order (FIFO by creation order unless otherwise specified).

### 4.10.3 Debugging Considerations

If debug hooks are enabled, the runtime SHOULD support selecting the active coroutine/thread for stack and locals inspection.

---

## 4.11 Hot Reload Semantics (v0.1)

### 4.11.1 Reload Modes

ARCANEE v0.1 SHALL implement **Full VM Reset Reload** as the default:

* Stop cartridge execution.
* Destroy VM.
* Recreate VM and re-mount `cart:/`.
* Reload entry script and call `init()` again.

### 4.11.2 State Persistence Rules

Full VM reset implies:

* All script state is lost.
* Runtime-owned resources MAY be retained only if explicitly designed as persistent across reload (v0.1 RECOMMENDS: free all cartridge-owned resources on reload to avoid leaks and confusing state).

### 4.11.3 Workbench UX Requirements

On reload:

* Workbench MUST clear prior error markers or annotate them as stale.
* Workbench MUST display compilation errors and stack traces with clickable file/line.

---

## 4.12 Error Handling and Reporting (Normative)

### 4.12.1 Runtime Errors in Cartridge Code

If `init/update/draw` throws an uncaught error:

* The runtime MUST capture:

  * message
  * source file and line
  * stack trace (best effort)
* The cartridge MUST transition to **Faulted** state.
* Workbench MUST show the error prominently and allow navigation to source.

### 4.12.2 API Errors

If an API function fails due to invalid arguments/permissions/handles:

* It MUST set `sys.getLastError()` with a human-readable message.
* It MUST return `false` or `null` (per API definition).
* In Dev Mode, it SHOULD also log a diagnostic line with details.

### 4.12.3 Logging

All cartridge prints/logs MUST be routed to Workbench console and optionally to a runtime log file (host policy).

---

## 4.13 Compliance Checklist (Chapter 4)

An implementation is compliant with Chapter 4 if:

1. It implements the cartridge lifecycle states and required transitions.
2. It creates exactly one VM per running cartridge and runs VM code only on the main thread.
3. It loads the entry script and modules via deterministic VFS-based canonical paths and caches modules.
4. It enforces `init/update/draw` semantics and fixed timestep behavior as specified.
5. It enforces sandbox restrictions (no OS access, VFS-only access, permission checks).
6. It implements time budget enforcement per update tick and provides hang protection behavior in Workbench.
7. It provides error reporting with canonical file/line and Workbench navigation.
8. It supports full VM reset hot reload as the default reload mechanism.