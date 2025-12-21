# Chapter 10 — Workbench UX, Hot Reload, Debugging, and Profiling (ARCANEE v0.1)

## 10.1 Overview

ARCANEE v0.1 is defined as a “modern home computer”: on startup the user enters the **Workbench**, an integrated environment that supports immediate cartridge development. This chapter specifies Workbench requirements, including:

* Startup behavior and project selection
* Integrated editor behavior (ImGui + ImGuiColorTextEdit)
* Run control, pause, reset, and hot reload semantics
* Error reporting, navigation, and diagnostics
* Debugging capabilities (internal hooks and optional external adapters)
* Profiling and performance tooling
* Dev Mode security constraints and separation from Player behavior

---

## 10.2 Workbench Startup and Project Model (Normative)

### 10.2.1 Startup Behavior

On launch, ARCANEE MUST enter Workbench mode. Workbench MUST provide:

* A “current project” (cartridge) loaded from a directory cartridge by default, OR
* A project picker to open/create a cartridge directory.

### 10.2.2 Project Structure Awareness

Workbench MUST display the cartridge contents using VFS namespaces:

* `cart:/` as the project source root (directory in Workbench)
* `save:/` as persistent storage (if permitted)
* `temp:/` as cache storage (optional visibility)

Workbench MUST prevent editing of `save:/` and `temp:/` from being confused with source code unless explicitly allowed.

### 10.2.3 File Watching (Recommended)

Workbench SHOULD watch project files for changes and update views accordingly. In v0.1:

* watching is advisory; manual reload is acceptable if stable.
* if implemented, file change detection must be cross-platform and robust.

---

## 10.3 Integrated Editor Requirements (Normative)

### 10.3.1 Editor Component

The integrated code editor SHALL be built using ImGui and ImGuiColorTextEdit (or equivalent) and MUST support:

* Syntax highlighting for Squirrel
* Line numbers
* Undo/redo
* Copy/paste
* Search (at minimum in-file search)

### 10.3.2 Navigation and Diagnostics

Workbench MUST support:

* Clickable error list (file:line) that navigates the editor cursor and scrolls to the location.
* Highlighting of the current line where an error occurred.
* Persistent display of file path in canonical `cart:/...` form.

### 10.3.3 Text Encoding

Editor MUST treat source files as UTF-8. Invalid UTF-8 must be surfaced to the user.

---

## 10.4 Run Control Model (Normative)

### 10.4.1 Controls

Workbench MUST provide at minimum:

* **Run**: load/compile/execute cartridge; call `init()`, then begin `update/draw`.
* **Stop**: stop cartridge execution; transition to Stopped; free cartridge-owned runtime resources.
* **Pause/Resume**: pause the cartridge (no updates); resume continues.
* **Reset**: equivalent to Stop then Run (full VM reset).

### 10.4.2 States and UI

Workbench MUST display cartridge state:

* Running / Paused / Faulted / Stopped
  and show relevant diagnostics when Faulted.

### 10.4.3 Output Windows

Workbench MUST provide:

* Console output window (logs, warnings, errors)
* Optional “Runtime status” panel (FPS, tick rate, present mode, CBUF size)

---

## 10.5 Hot Reload (Normative)

### 10.5.1 Reload Definition

In v0.1, **hot reload** MUST be implemented as a **Full VM Reset Reload** (Chapter 4):

* Destroy VM
* Recreate VM
* Re-mount `cart:/`
* Reload entry script and all required modules
* Re-run `init()`

### 10.5.2 Reload Triggering

Workbench MUST expose:

* Manual reload command (button and shortcut)
* Optional auto-reload on file save (user-configurable)

### 10.5.3 Reload Error Behavior

If reload fails due to compilation/runtime error:

* Cartridge enters Faulted state
* Workbench shows the error list and stack trace
* The previous running instance MUST NOT continue (no “partial reload” in v0.1)

---

## 10.6 Error Reporting and Stack Traces (Normative)

### 10.6.1 Runtime Error Capture

When cartridge code errors (init/update/draw or module load):
Workbench MUST display:

* Error message
* Canonical source path `cart:/...`
* Line number
* Call stack (best effort)

### 10.6.2 Click-to-Navigate

Each stack frame (file:line) MUST be clickable to navigate in the editor.

### 10.6.3 Last Error Channel

Workbench MAY also expose `sys.getLastError()` values from API failures in a non-spammy way.

---

## 10.7 Debugging (Dev Mode) (Normative)

### 10.7.1 Dev Mode Definition

Dev Mode is enabled in Workbench by default and is considered a privileged environment. However:

* Dev Mode features MUST NOT be enabled in Player Mode unless explicitly configured.

### 10.7.2 Minimum Debugging Features (v0.1)

ARCANEE v0.1 MUST provide at least:

* Pause/Resume execution
* Inspect current cartridge state (frame count, tick rate, console size)
* View recent logs and errors

Additionally, ARCANEE v0.1 SHOULD provide:

* Step-over/Step-into/Step-out
* Breakpoints by file:line
* Call stack and locals view

If advanced debugging is not implemented, the runtime must still support:

* debug metadata compilation in Dev profile
* stable canonical source naming for future integration

### 10.7.3 Internal Debugger (Optional)

If implemented, an internal debugger MUST:

* Integrate with the tick loop such that “pause” prevents `update()` execution.
* Allow stepping at line granularity only when debug metadata is enabled.
* Provide a locals view for the selected stack frame where feasible.

### 10.7.4 External Debug Adapter (Optional)

ARCANEE MAY integrate an external debug adapter (e.g., DAP-based) in Dev Mode. If so, the following security constraints are normative:

* The adapter MUST listen on `127.0.0.1` only by default.
* The adapter MUST be disabled by default in Player Mode.
* Workbench MUST clearly indicate when an external debug port is open.
* A session token or equivalent authentication SHOULD be used even for localhost.

---

## 10.8 Profiling and Performance Tooling (Normative)

### 10.8.1 CPU Profiling

Workbench MUST display per-frame CPU timings at minimum:

* Frame time
* Total update time (sum over all update ticks)
* Draw time (script-side)
* Render submission time (host-side)

### 10.8.2 GPU Profiling (Recommended)

If supported by backend, Workbench SHOULD display GPU timings for passes:

* 3D pass
* 2D upload/composite pass
* Present pass
* UI pass

If GPU timing is not available, Workbench must still show CPU render submission time.

### 10.8.3 Counters and Limits

Workbench MUST show key runtime counters:

* CBUF preset and aspect
* Present mode (fit/integer_nearest/fill/stretch)
* Scaling factor `k` when integer scaling is enabled
* Draw call count (approximate acceptable)
* Resource counts (textures, meshes, sounds)

### 10.8.4 Profiling Markers

ARCANEE SHOULD expose Dev-only profiling markers for cartridge code:

* `dev.profileBegin(name)`
* `dev.profileEnd(name)`
  Markers must be no-ops in Player mode.

---

## 10.9 Runtime Configuration UI (Normative)

Workbench MUST provide UI controls for:

* Console resolution preset (Low/Medium/High/Ultra)
* Aspect mode (16:9 / 4:3) where permitted
* Present/scaling mode (Fit / Integer Nearest / Fill / Stretch)
* Filter mode for non-integer scaling (Linear vs Nearest) (recommended)
* VSync toggle (Workbench default on)
* Audio master volume and optional latency controls

If `display.allow_user_override=false`, these controls MUST be disabled or operate only as temporary preview overrides with clear indication.

---

## 10.10 Logging, Console, and Export (v0.1)

### 10.10.1 Console

Workbench MUST provide:

* log filtering by severity (log/warn/error)
* search within console output
* ability to copy log lines

### 10.10.2 Export / Package (Optional in v0.1)

Workbench MAY provide “Export cartridge to archive”.
If implemented:

* the archive must include manifest, source, and assets under `cart:/`
* save data MUST NOT be included by default

---

## 10.11 Player Mode Differences (Normative)

In Player Mode:

* Workbench editor UI is disabled or not shown.
* Dev Mode debugging/profiling UI is disabled by default.
* External debug ports MUST be disabled by default.
* Present mode and CBUF presets may be locked to manifest or user policy.

---

## 10.12 Compliance Checklist (Chapter 10)

An implementation is compliant with Chapter 10 if it:

1. Boots into Workbench mode with a project selection/open flow.
2. Provides an integrated editor with syntax highlighting, line numbers, undo/redo, and search.
3. Provides run/pause/stop/reset controls and displays cartridge state.
4. Implements hot reload as full VM reset and surfaces errors with clickable file:line navigation.
5. Provides a console log window and diagnostic panels (at minimum CPU timings and key render settings).
6. Enforces Dev Mode separation from Player mode and keeps any debug adapter local-only by default.
---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
