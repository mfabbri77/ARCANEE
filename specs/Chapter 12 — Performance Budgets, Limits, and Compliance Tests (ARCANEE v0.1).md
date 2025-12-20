# Chapter 12 — Performance Budgets, Limits, and Compliance Tests (ARCANEE v0.1)

## 12.1 Overview

ARCANEE v0.1 must remain responsive and stable even under misbehaving cartridges. This chapter defines:

* Default budgets and limits (CPU, VM, GPU-facing resources, canvas complexity, audio)
* Enforcement mechanisms and required failure behavior
* Workbench diagnostics and user-facing controls
* Compliance and conformance tests to validate an implementation

Budgets are expressed as **policy defaults**. The runtime remains authoritative and may clamp cartridge-requested caps.

---

## 12.2 Budget Domains (Normative)

ARCANEE enforces limits across these domains:

1. **CPU budgets**

   * per `update()` tick time budget
   * optional watchdog timeout for hangs
   * max update steps per frame

2. **VM budgets**

   * maximum VM memory (soft/hard) and/or allocation limits
   * recursion depth / call stack limits (optional but recommended)

3. **Rendering budgets**

   * draw call count (approximate)
   * total pixels processed for canvas (target pixel count)
   * per-frame path complexity (segments)
   * surface counts and sizes
   * texture counts and total texture memory estimate

4. **3D budgets**

   * max entities, max mesh renderers, max lights
   * max triangles (soft limit) and mesh count
   * texture memory estimate

5. **Storage budgets**

   * `save:/` quota (bytes)
   * `temp:/` quota (bytes)
   * max file count (optional)

6. **Audio budgets**

   * max concurrent SFX voices
   * max loaded sound memory
   * max module size (policy)

---

## 12.3 Default Policy Values (Recommended Baselines)

The following defaults are RECOMMENDED for ARCANEE v0.1. Implementations MAY adjust but MUST document differences.

### 12.3.1 CPU and Scheduling Defaults

* `tick_hz`: 60
* `max_updates_per_frame`: 4
* `cpu_ms_per_update` (soft budget):

  * Workbench default: 2.0 ms
  * Player default: 2.0–4.0 ms (policy-driven)
* `hang_watchdog_ms` (hard):

  * Workbench default: 200 ms
  * Player default: 500 ms or disable interrupt (policy-driven)

### 12.3.2 VM Defaults

* `vm_memory_mb` (soft cap): 64 MB
* `vm_memory_mb` (hard cap): 128 MB (recommended)
* `max_callstack_depth`: 256 (recommended)
* `max_table_size` / `max_array_size`: policy-defined (optional)

### 12.3.3 2D Canvas Defaults

* `max_canvas_pixels`:

  * For a single target: max 16,777,216 pixels (e.g., 4096×4096)
* `max_surfaces`: 32
* `max_surface_pixels_total`: 33,554,432 (sum of all surfaces)
* `max_path_segments_per_frame`: 100,000 (soft limit; warn) / 250,000 (hard fail)
* `max_save_stack_depth`: 64

### 12.3.4 3D Defaults

* `max_entities`: 50,000 (soft; Workbench warns)
* `max_mesh_renderers`: 10,000 (soft)
* `max_lights`: 256 (soft; runtime may clamp to smaller)
* `max_visible_lights_per_object`: 8 (hard clamp, deterministic)
* `max_triangles_per_frame`: policy-defined soft (e.g., 5–20 million depending on preset), warn only in v0.1

### 12.3.5 Storage Defaults

* `save_quota_mb`: 50 MB
* `temp_quota_mb`: 200 MB (purgeable)
* `max_files_save`: 10,000 (recommended)
* `max_path_length`: 240 characters (recommended cross-platform)

### 12.3.6 Audio Defaults

* `audio_channels` (voices): 32
* `max_sound_memory_mb`: 64 MB
* `max_module_size_mb`: 128 MB (recommended)

---

## 12.4 Enforcement Policies (Normative)

### 12.4.1 CPU Budget Enforcement

For each `update(dt_fixed)` call:

* Measure elapsed CPU time.
* If elapsed > `cpu_ms_per_update`:

  * Workbench: increment overrun counter, log warning with timing, optionally highlight in profiler.
  * If overruns exceed threshold (e.g., 10 consecutive or 30 in a minute), Workbench SHOULD auto-pause and surface a “Performance Overrun” panel.

Player policy MUST be defined (one of):

* continue with warnings disabled (silent)
* auto-pause
* auto-stop (enter faulted)

### 12.4.2 Hang Watchdog Enforcement

If `update()` does not return within `hang_watchdog_ms`:

* Workbench MUST interrupt execution if feasible or terminate the VM and enter Faulted state.
* A clear error report MUST be shown (HangDetected).

### 12.4.3 VM Memory Enforcement

The runtime MUST track VM memory usage:

* If memory exceeds soft cap: warn (Workbench) and continue.
* If memory exceeds hard cap: fail the allocating operation or fault the cartridge deterministically.

If exact memory tracking is not possible, the runtime MUST implement an approximate but safe policy and document it.

### 12.4.4 Rendering Limit Enforcement

The runtime MUST enforce:

* `max_canvas_pixels` and surface pixel limits: creation fails if exceeded.
* `max_path_segments_per_frame`: when exceeded, the runtime MUST either:

  * fail subsequent path operations for the frame, or
  * clamp deterministically and warn (Workbench).
    The policy MUST be deterministic and documented.

Draw call limits:

* The runtime SHOULD track draw call counts and warn/fail based on caps.

### 12.4.5 Storage Quota Enforcement

Writes that exceed quota MUST fail safely and set last error (Chapter 11).

### 12.4.6 Audio Enforcement

When exceeding max voices:

* The runtime MUST apply a deterministic voice stealing policy (Chapter 8).
  When exceeding sound/module size/memory:
* Loading must fail safely.

---

## 12.5 Workbench Diagnostics Requirements (Normative)

Workbench MUST provide a “Limits & Budgets” panel showing:

* Current effective caps (after runtime policy clamping)
* Current usage (VM memory estimate, surface pixel totals, loaded textures, audio voices)
* Overrun counters (CPU update overruns, hang incidents)
* Storage usage (save/temp usage vs quota)

When a limit is exceeded, Workbench MUST:

* highlight the category
* provide a clear explanation and recommended mitigation

---

## 12.6 Configuration and Overrides (Normative)

### 12.6.1 Cartridge-Requested Caps

Cartridge manifest `caps` is advisory. The runtime:

* MAY clamp caps to policy
* MUST expose the effective caps in Workbench
* MUST apply caps consistently

### 12.6.2 User Policy

User settings may define stronger restrictions (e.g., for low-end devices). Such settings MUST be deterministic and stored outside cartridges.

### 12.6.3 Present Mode and Preset Overrides

When `display.allow_user_override=false`, Workbench MUST not persist overrides. Temporary preview overrides MAY exist but must be clearly indicated and not saved into cartridge manifest automatically.

---

## 12.7 Compliance Tests (Normative)

ARCANEE v0.1 implementations MUST include (or be verifiable by) a conformance test suite. The suite can be internal, but it MUST validate the following behaviors.

### 12.7.1 VFS and Sandbox Tests

1. Attempt to read/write outside namespaces → must fail.
2. Attempt `..` path traversal → must fail.
3. `fs.listDir` ordering is stable and lexicographical.

### 12.7.2 Deterministic Tick and Input Tests

1. Fixed timestep update count under varying frame rates.
2. Input snapshot stability inside one tick.
3. Pressed/released edge correctness.

### 12.7.3 Present Modes and Integer Scaling Tests

1. For each present mode, verify viewport math exactly as spec:

   * correct `vpX/vpY/vpW/vpH` for known `W,H,w,h`.
2. For `integer_nearest`, verify:

   * integer scaling factor `k` selection,
   * point sampling enabled,
   * no mip usage for CBUF sample.
3. Verify `mousePos` mapping respects viewport and returns (-1,-1) outside.

### 12.7.4 Budget Enforcement Tests

1. A cartridge that intentionally runs long in `update()` triggers:

   * Workbench overrun warnings,
   * optional pause policy after threshold.
2. An infinite loop triggers hang watchdog behavior.
3. Canvas surface allocation exceeding limits fails with correct error.

### 12.7.5 Handle Safety Tests

1. Use freed handle → API fails safely, sets last error (no crash).
2. Pass wrong handle type to API → fails safely.

### 12.7.6 Audio Tests

1. Load and play module; verify state transitions.
2. Exceed voice limit; verify deterministic voice stealing.
3. Device loss simulation (if possible) results in safe failure.

---

## 12.8 Final Conformance Checklist (ARCANEE v0.1)

An implementation is ARCANEE v0.1 conformant if it satisfies:

* All MUST requirements across Chapters 1–12
* Provides Workbench with run control, editor navigation, hot reload
* Implements CBUF and present modes including integer scaling with nearest
* Implements sandboxed VFS and permission checks
* Provides stable APIs and safe failure behavior
* Enforces budgets and provides Workbench diagnostics
* Passes the conformance test suite defined in §12.7