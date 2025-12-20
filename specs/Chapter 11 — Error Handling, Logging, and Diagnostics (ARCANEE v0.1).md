# Chapter 11 — Error Handling, Logging, and Diagnostics (ARCANEE v0.1)

## 11.1 Overview

ARCANEE v0.1 must be robust under faulty cartridges, malformed assets, and runtime subsystem failures. This chapter defines a uniform error and diagnostics model across:

* Squirrel execution (compile/runtime errors)
* Public API failures (`sys/fs/inp/gfx/gfx3d/audio/dev`)
* VFS and permissions (PhysFS)
* Rendering and device issues (Diligent)
* Audio device failures (SDL + backend)
* Workbench UX error surfacing and developer diagnostics

The goals are:

1. **Fail safely**: no crash due to cartridge actions.
2. **Actionable errors**: clear messages, canonical `cart:/` paths, and file:line navigation.
3. **No information leaks**: do not expose host filesystem paths or sensitive system info in Player mode.
4. **Deterministic behavior**: errors and warnings must not introduce nondeterminism.

---

## 11.2 Error Taxonomy (Normative)

ARCANEE classifies errors into the following categories:

### 11.2.1 Cartridge Script Errors

* **CompileError**: syntax/compile failure of a `.nut` file.
* **RuntimeError**: uncaught exception during `init/update/draw` or module execution.
* **MissingEntryPoints**: missing required `init/update/draw`.
* **ModuleLoadError**: failure during `require` resolution/compile/execute.

### 11.2.2 API Usage Errors

* **InvalidArgument**: wrong type/arity or invalid numeric ranges.
* **InvalidHandle**: unknown, freed, or wrong-type handle.
* **PermissionDenied**: operation disallowed by manifest permissions or user policy.
* **QuotaExceeded**: exceeded a runtime-enforced quota (storage, memory, resource count).
* **UnsupportedFeature**: API exists but is not implemented in v0.1 or in the current backend configuration.

### 11.2.3 Subsystem Errors

* **IOError**: VFS read/write/stat failure.
* **AssetDecodeError**: image/font/model/audio decode failure.
* **DeviceError**: graphics device lost, swapchain failure, shader compilation failure.
* **AudioDeviceError**: audio device unavailable, stream failure, backend init failure.

### 11.2.4 Budget/Watchdog Errors

* **UpdateBudgetOverrun**: CPU time budget exceeded for `update()`.
* **HangDetected**: update call exceeded watchdog threshold.
* **RenderBudgetOverrun** (optional): frame time or draw call budgets exceeded.

---

## 11.3 The Last-Error Channel (`sys.getLastError`) (Normative)

### 11.3.1 Semantics

ARCANEE provides a **thread-confined, cartridge-scoped last-error string**:

* `sys.getLastError() -> string|null`
* `sys.clearLastError() -> void`

Rules:

* The last-error value is set by the most recent failing API call on the main thread.
* A successful API call MUST NOT clear last-error automatically.
* `clearLastError()` explicitly clears it.
* The last-error string MUST be human-readable and suitable for Workbench display.

### 11.3.2 Privacy Requirements

In Player Mode, last-error strings MUST NOT include:

* absolute host filesystem paths
* OS usernames
* detailed GPU driver identifiers beyond basic backend name unless user opted in

In Workbench Dev Mode, detailed diagnostics MAY be available in logs, but UI-visible messages should still prefer canonical `cart:/...` paths.

---

## 11.4 Logging Model (Normative)

### 11.4.1 Log Levels

ARCANEE defines log severity levels:

* `INFO`
* `WARN`
* `ERROR`

Cartridge-facing logging API:

* `sys.log(msg)`
* `sys.warn(msg)`
* `sys.error(msg)`

### 11.4.2 Log Sinks

ARCANEE MUST provide at least these sinks in Workbench:

1. **Workbench Console Window** (primary)
2. **In-memory ring buffer** for recent logs (for crash reporting / quick access)

Optional sinks:

* runtime log file (user-configurable)
* structured telemetry (out of scope for v0.1)

### 11.4.3 Timestamping and Ordering

* Logs MUST be timestamped (monotonic time) and include the frame count.
* Logs MUST preserve call order.

### 11.4.4 Spam Control

To prevent console flooding, the runtime SHOULD implement:

* per-frame log line limits (policy-defined)
* message coalescing for repeated identical messages (optional but recommended)

This must be deterministic: e.g., “log suppression after N per frame” must always behave the same given identical execution.

---

## 11.5 Script Error Reporting (Normative)

### 11.5.1 Required Fields

For compile/runtime errors, Workbench MUST be able to display:

* error type (CompileError/RuntimeError/etc.)
* message
* canonical source name `cart:/...`
* line number
* stack trace (best effort)

### 11.5.2 Stack Trace Format

ARCANEE SHOULD represent stack traces as an array of frames:

* `{ file: "cart:/src/x.nut", line: int, func: string|null }`

Each frame MUST be clickable in Workbench to navigate to file:line.

### 11.5.3 Fault Handling Policy

Uncaught errors in `init/update/draw` MUST:

* transition cartridge to **Faulted** state
* halt further `update()` calls
* allow Workbench to show the last rendered frame (optional)
* allow the developer to Reset/Reload

---

## 11.6 API Failure Reporting (Normative)

### 11.6.1 Return Conventions

On failure:

* handle-returning functions return `0`
* boolean functions return `false`
* table-returning functions return `null`
* void functions become no-ops but MUST set last-error (Dev Mode should log)

### 11.6.2 Error Message Requirements

Last-error messages MUST include:

* the API function name (or module name) in Workbench Dev Mode (optional in Player mode)
* a short, user-readable cause

Example (Workbench):

* `"gfx.drawImageRect: invalid ImageHandle (0)"`

Example (Player):

* `"invalid image handle"`

### 11.6.3 Unsupported Features

For APIs reserved for future versions:

* functions MUST exist
* calls MUST fail with `UnsupportedFeature`
* error message should include the required engine/api version if appropriate

---

## 11.7 Subsystem Diagnostics (Normative)

### 11.7.1 VFS Diagnostics

On VFS errors, Workbench logs SHOULD include:

* operation (read/write/list/stat)
* normalized VFS path
* error code from PhysFS (if available) in Dev Mode

Player mode MUST avoid host path leakage.

### 11.7.2 Rendering Diagnostics

Graphics failures (swapchain recreation, shader compilation, device loss) MUST:

* surface a clear Workbench error panel if in Workbench
* attempt automatic recovery when possible
* fail gracefully (e.g., show fallback screen) rather than crashing

### 11.7.3 Audio Diagnostics

Audio device failures MUST:

* show status indicator in Workbench
* allow continued non-audio execution where possible
* ensure `audio.*` fails safely when the device is unavailable

---

## 11.8 Crash Safety and Post-Mortem Reporting (v0.1 Requirements)

### 11.8.1 Crash Containment

ARCANEE MUST attempt to contain cartridge faults without crashing the host process. If a host crash occurs anyway:

* the runtime SHOULD preserve the last N log lines in memory and flush them to disk on next start (optional in v0.1, recommended)

### 11.8.2 Minimal Crash Report (Recommended)

Workbench SHOULD offer a crash report containing:

* engine version
* platform/backend (DX12/Vulkan/Metal)
* last cartridge id
* last error/stack trace if available
* last N log lines

---

## 11.9 Determinism Considerations

Logging and diagnostics MUST NOT affect simulation outcomes. Therefore:

* logging must not change timing passed to `update()`
* diagnostic rendering (e.g., overlays) must not change cartridge-visible state, except in Dev Mode where debugging may pause execution

---

## 11.10 Compliance Checklist (Chapter 11)

An implementation is compliant with Chapter 11 if it:

1. Implements a uniform error taxonomy and safe failure returns for all APIs.
2. Provides `sys.getLastError()` and enforces privacy constraints between Workbench and Player mode.
3. Provides Workbench console logging with stable ordering and timestamps.
4. Captures compile/runtime errors with canonical file paths and clickable stack traces.
5. Surfaces subsystem failures (VFS, graphics, audio) to Workbench and fails safely without crashing.
6. Implements deterministic spam control policy or documents absence.