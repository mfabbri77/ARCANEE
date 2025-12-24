# Blueprint v0.1 — Chapter 6: Concurrency, Determinism, GPU/Audio Sync

## 6.1 Thread Model (v1.0)
- [CONC-01] Threads:
  - Main/UI thread: Runtime tick, VM calls, GPU submission, Workbench UI.
  - Audio callback thread: mixes from shared buffers only.
  - Optional background file-watcher thread (Workbench) (may be polling-based).

## 6.2 Synchronization Rules
- [REQ-40] Audio thread must not lock contended mutexes in callback; use:
  - lock-free ring buffer, or
  - single-producer/single-consumer queue with bounded capacity.
- [REQ-49] Any cross-thread communication must be bounded and drop/overwrite with metrics, not block indefinitely.

## 6.3 Determinism Rules (v1.0)
- [REQ-21] Deterministic simulation requires:
  - deterministic input snapshot ordering
  - deterministic RNG stream (seeded; no `std::random_device` in simulation path)
  - deterministic time step (fixed dt)
- [REQ-50] For v1.0, determinism tests compare:
  - serialized sim state hash and/or canonical event log hash
  - not rendered frames or audio sample streams
- [TEST-05] implements deterministic replay smoke.

## 6.4 GPU Sync Model
- [REQ-51] Render submission is single-threaded (main thread). GPU fences are used only where required:
  - resource upload staging completion
  - swapchain present pacing (backend-defined)
- [DEC-24] Avoid explicit multi-queue scheduling in v1.0.
  - Rationale: reduce complexity; Diligent abstracts backend details.
  - Alternatives: multi-threaded rendering, async compute.
  - Consequences: performance ceiling; revisit vNEXT.

## 6.5 Concurrency Testing
- [TEST-17] TSan build (where supported) runs `workbench_smoke` and `runtime_smoke`.
- [TEST-18] Stress: rapid hot-reload while running for N seconds must not crash/leak.


