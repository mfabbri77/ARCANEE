<!-- GPU_APIs_rules.md -->

# GPU APIs Backend Rules (Normative)
This document defines **best-practice rules** for implementing GPU backends inside a **multi-backend native C++** project governed by the DET CDD blueprint system. It consolidates backend-specific rules for **DirectX 12**, **Vulkan**, and **Metal**, and provides cross-cutting requirements that enable deterministic evolution (v1→v2+), strong contracts, and CI-enforced correctness.

> **Precedence (within the overall system):** Prompt hard rules → `blueprint_schema.md` → this document → project-specific rulefiles → user constraints → DEC.

---

## 0. Applicability and required blueprint hooks (MUST)

When TOPIC includes `gpu` (or GPU is being evaluated), the blueprint MUST include a GPU decision (DEC) that explicitly states:
- **Backend selection**: Vulkan and/or Metal and/or DirectX 12 (or “no GPU”).
- **Target platform matrix**: OS baselines, GPU vendor classes, minimum driver/SDK.
- **Feature subset**: what features are supported vs out-of-scope.
- **Abstraction boundary**: what is backend-agnostic vs backend-specific.
- **Threading and synchronization model**: command recording, submission, resource lifetime, hazards.
- **Error model**: mapping backend errors to project errors, device lost policy.
- **Testing strategy**: smoke + negative + stress + (optional) perf; CI feasibility plan.
- **Observability**: logging/metrics/tracing budgets and debug tooling integration.

If “no GPU” is selected, the blueprint MUST add **compile-time** and **runtime** exclusion gates:
- no GPU headers/libs included,
- no GPU runtime dependencies shipped,
- no GPU code paths reachable in production binaries.

---

## 1. Glossary (short)

- **Backend**: a concrete implementation using a specific API (DX12/Vulkan/Metal).
- **Core**: backend-agnostic engine layer that uses a unified internal interface.
- **Public API**: exported headers under `/include/<proj>/...` (subject to `cpp_api_abi_rules.md`).
- **Hot path**: latency-critical per-frame/per-tick path (see Ch5 in blueprint).
- **Device-lost**: GPU reset/removal scenario requiring explicit policy.

---

## 2. How to extend this file (for new GPU APIs) (MUST)
To add support for a new GPU API (e.g., WebGPU, CUDA interop, OpenGL), you MUST:
1. Add a new top-level section: `## <API Name> Backend Rules (Normative)`.
2. Preserve the same internal structure as existing backends:
   - Boundary / leakage rules
   - Device selection + capability gating
   - Resource lifetime + ownership
   - Synchronization / hazards
   - Binding/descriptor model
   - Pipeline + shader strategy
   - Present/swapchain (if applicable)
   - Debug/diagnostics
   - Error handling + device lost policy
   - Threading model
   - Testing requirements
   - Quick checklist
3. Put cross-cutting rules in **Common GPU Backend Rules** only.

---

## 3. Common GPU Backend Rules (Applies to all backends) (MUST)

### 3.A Backend isolation and header hygiene
- Each backend MUST live in its own dedicated directory and build target:
  - examples: `<proj>_vk`, `<proj>_dx12`, `<proj>_mtl`.
- Core MUST depend on backends only via a stable internal interface (factory/registry/service locator).
- Backend headers MUST NOT leak platform/GPU headers into core or public headers, unless explicitly part of the public API.
  - Public headers under `/include/<proj>/...` MUST NOT include:
    - `windows.h`, `d3d12.h`, `dxgi.h`
    - `<vulkan/vulkan.h>`
    - `<Metal/Metal.h>` / ObjC headers
  unless a DEC declares that backend-native objects are part of the SDK surface.

**Rationale:** prevents accidental platform lock-in and reduces rebuild churn.

### 3.B Deterministic backend selection and capability gating
- Backend selection MUST be deterministic:
  - explicit configuration option (compile-time or runtime),
  - deterministic device selection rules,
  - log/report chosen backend + device + key capabilities.
- Capabilities MUST be captured at startup into a **capability struct** (immutable after init) and used to gate feature usage.
- Optional features MUST NEVER be assumed present due to “common hardware” heuristics.

**Minimum required capability fields (recommended):**
- API version / feature level
- shader model / language level
- resource binding tier / descriptor indexing support
- supported formats used by the project
- queue families / queue types available
- timestamp queries availability and precision (if used)
- memory model constraints (UMA vs discrete, host-visible coherence behavior)
- presentation support (if applicable)

### 3.C Hot-path discipline (allocations, creation, blocking)
- GPU resource creation (pipelines, descriptor sets/heaps, PSOs) MUST be avoided on the hot path.
- Per-draw/per-dispatch allocations are forbidden on the hot path unless DEC + perf gates.
- CPU waits for GPU completion are forbidden on the hot path unless DEC + perf gates.
- Prefer:
  - pooling/suballocation for descriptors and transient buffers,
  - per-frame contexts with ring buffers,
  - pipeline/PSO caches.

### 3.D Resource lifetime and shutdown ordering
- All backend resources MUST be managed via RAII with explicit ownership.
- Shutdown order MUST be deterministic and documented (device/queue last or first per API rules; choose and enforce).
- Destruction MUST be safe with in-flight GPU work:
  - resources MUST not be freed while GPU can still reference them.
  - reclamation MUST be fence-guarded (or equivalent completion tracking).

### 3.E Synchronization and hazard correctness
- The blueprint MUST choose a synchronization model and document it:
  - “single graphics queue” vs “multi-queue”
  - per-frame fence model
  - timeline semaphores (where available) vs binary sync
- A resource state/hazard tracking strategy MUST be chosen and implemented:
  - explicit tracking (DX12 required; Vulkan strongly recommended),
  - layout tracking for images (Vulkan/Metal),
  - upload/download visibility rules (CPU↔GPU).

### 3.F Error handling and device lost policy
- Backend-specific errors MUST be mapped to canonical project errors per `error_handling_playbook.md`.
- Preserve raw backend error code as `subcode` (e.g., HRESULT/VkResult) for diagnostics.
- Device-lost/removal MUST have an explicit policy:
  - recover by device recreation (with state restoration), OR
  - fail-fast with clear message and crash diagnostics.
- The policy MUST be testable (as feasible) and observable (logs/metrics).

### 3.G Shader and pipeline portability strategy
A project MUST choose (DEC) a shader strategy that works across selected backends:
- source language (HLSL/GLSL/MSL) and compilation pipeline,
- intermediate representation(s) (SPIR-V, DXIL),
- reflection strategy (offline reflection preferred for determinism),
- stable binding layout rules across backends.

Rules:
- Runtime shader compilation in shipping builds is discouraged; if allowed, requires DEC + performance budget + caching strategy.
- Shader interface changes (bindings, vertex formats) MUST be treated as versioned changes; provide migration notes if exposed.

### 3.H Present / swapchain policy (when applicable)
If presentation is in scope:
- The project MUST define swapchain policy:
  - format selection
  - colorspace
  - vsync policy
  - resize handling strategy
- Resize and surface loss handling MUST be robust and leak-free.

If headless/offscreen operation is in scope:
- Define a headless path per backend (recommended for CI).

### 3.I Observability, diagnostics, and redaction
- GPU debug output MUST route through the project logging policy (`logging_observability_policy_new.md`).
- Debug markers (RenderDoc/PIX/Xcode groups) SHOULD be enabled in debug builds and disabled/sampled in release builds.
- Ad-hoc debug prints MUST follow `temp_dbg_policy.md`.
- Logs MUST avoid unbounded dumps of shaders/resources unless DEC + redaction.

### 3.J Minimal test baseline (MUST)
Blueprint MUST define tests for:
- **init smoke**: device creation + minimal submission (no present required)
- **resource lifetime stress**: create/destroy many resources; detect leaks
- **sync correctness smoke**: fences/semaphores correctness for at least one frame loop
- **present/resize** (if applicable): create, present, resize, recreate, present again
- **device lost policy**: at minimum, a simulated failure path test (see backend sections)

CI-friendly headless/offscreen mode is RECOMMENDED.

---

## 4. Cross-backend layering model (Recommended, becomes MUST when multi-backend)

### 4.1 Layering (recommended)
- **Core** defines backend-agnostic interfaces:
  - `IGpuDevice`, `ICommandQueue`, `ICommandList/Encoder`, `IBuffer`, `ITexture`, `IPipeline`, `IDescriptorSet/BindGroup`, `ISwapchain`.
- **Backend** implements these interfaces and owns native objects.
- **Translation layer** handles:
  - shader reflection translation,
  - resource state translation,
  - platform surface integration.

### 4.2 Handle leakage policy
- Public API SHOULD remain backend-agnostic.
- If native handles must be exposed (advanced integration):
  - expose via opaque handle wrappers,
  - guard by compile-time feature flags,
  - document ABI policy and platform coupling in Ch4.

### 4.3 Compile-time vs runtime backend selection
- Compile-time selection reduces footprint and improves determinism.
- Runtime selection improves portability and can enable fallback.
- If runtime selection is used:
  - selection must be deterministic (config-driven),
  - dynamic loading must be version-pinned and gated,
  - backend absence must fail gracefully with actionable error.

---

## 5. DirectX 12 Backend Rules (Normative)

### 5.1 Backend boundary (MUST)
- DX12 code MUST live under a dedicated backend target and folder (e.g., `/src/backends/dx12/`).
- Core/public headers MUST NOT include `windows.h`, `d3d12.h`, `dxgi.h` unless explicitly part of public API.

### 5.2 Device & adapter selection (MUST)
- Enumerate adapters via DXGI.
- Provide deterministic adapter selection:
  - prefer high-performance adapter by default,
  - allow explicit selection by LUID/index,
  - log: adapter name, vendor/device id, memory, driver version (where feasible).
- Pin minimum `D3D_FEATURE_LEVEL` and document it.
- Query and store capability struct including:
  - resource binding tier
  - descriptor heap sizes
  - shader model requirements
  - optional features (mesh shaders, sampler feedback, etc.) only if used

### 5.3 COM and lifetime management (MUST)
- Use RAII for COM objects (`ComPtr` or equivalent).
- No manual `Release()` except in rare, justified cases (DEC).
- Deterministic shutdown order MUST be documented:
  - ensure GPU is idle or fences guarantee no in-flight references before teardown.

### 5.4 Synchronization model (MUST)
- Use fences for CPU↔GPU synchronization.
- Avoid CPU waits on hot path; use frame-latency buffers and fence-guarded reclamation.
- If multiple queues (graphics/compute/copy) are used:
  - define cross-queue ownership and sync,
  - use fences or queue sync primitives explicitly.

### 5.5 Resource states and barriers (MUST)
- Implement explicit per-resource state tracking (DX12 requires this).
- Track at least:
  - `D3D12_RESOURCE_STATES` current state
  - whether tracking is per-resource vs per-subresource (choose and enforce)
- Batch barriers; avoid “transition to COMMON everywhere” anti-pattern.
- Every usage MUST be preceded by correct state transition unless proven-safe.

### 5.6 Descriptor heaps and binding strategy (MUST)
Blueprint MUST choose a descriptor strategy:
- CPU staging heaps + GPU-visible ring heap, OR
- global GPU-visible heap with suballocation, OR
- per-frame heap reset model.

Rules:
- No per-draw descriptor allocations.
- Stable root signatures (treat changes as high-impact).
- Descriptor heap exhaustion must be detectable and handled gracefully (error or fallback).

### 5.7 Pipeline state and shader strategy (MUST)
- Shader compilation policy MUST be defined:
  - offline to DXIL recommended for shipping,
  - runtime compilation allowed only for dev tools unless DEC.
- PSO creation MUST not occur on hot path; cache PSOs keyed by:
  - shader bytecode hashes + render state + root signature.
- Optional disk cache MUST define invalidation rules (driver change, shader change, version).

### 5.8 Command lists and allocators (MUST)
- Use per-thread/per-frame command allocators to avoid contention.
- Reset allocators only after GPU completion (fence-guarded).
- Clearly define frame contexts.

### 5.9 Swap chain and present (if applicable) (MUST)
- Use flip model swap chain (recommended) unless constraints require otherwise (DEC).
- Resize handling MUST:
  - wait for safe point (fence),
  - release back buffers,
  - resize buffers,
  - recreate RTVs/DSVs,
  - restore rendering state.

### 5.10 Debugging and diagnostics (SHOULD)
- Enable D3D12 debug layer in dev/CI builds.
- Integrate DRED (Device Removed Extended Data) for device removed diagnostics.
- Name resources and command lists in debug builds.
- PIX event markers SHOULD be used in debug builds.

### 5.11 Error handling and device removed (MUST)
- Check all HRESULTs.
- Map HRESULT → canonical errors; preserve HRESULT as `subcode`.
- Handle `DXGI_ERROR_DEVICE_REMOVED/RESET` via explicit policy:
  - capture DRED, log, and either recover or fail-fast.

### 5.12 Threading model (MUST)
Blueprint MUST define:
- which threads record command lists,
- submission thread model,
- locking strategy for shared caches (PSO, descriptor allocators).

Rules:
- Avoid global locks on hot path.
- Prefer per-thread contexts and lock-free submission queues if needed.

### 5.13 DX12 testing checklist (MUST)
- init smoke: factory/adapter/device creation
- queue + fence correctness test
- resource create/destroy stress + leak check
- barrier correctness smoke
- swapchain present + resize (if applicable)
- device removed handling (as feasible; at least simulated error injection path)

---

## 6. Vulkan Backend Rules (Normative)

### 6.1 Backend boundary (MUST)
- Vulkan code MUST live under a dedicated backend target and directory (e.g., `/src/backends/vulkan/`).
- Core/public headers MUST NOT include `<vulkan/vulkan.h>` unless explicitly part of public API.

### 6.2 Loader/dispatch strategy (MUST)
Blueprint MUST choose one approach and enforce it:
- platform loader directly, OR
- a loader helper (e.g., volk) (allowed if dependency policy approves)

Rules:
- Dispatch loading MUST be done once, thread-safe, and deterministic.

### 6.3 Instance/device creation policy (MUST)
- Separate “required” vs “optional” instance extensions/layers.
- Debug builds SHOULD enable:
  - `VK_EXT_debug_utils`
  - validation layers (when available)
- Physical device selection MUST be deterministic:
  - define preference order (discrete vs integrated, VRAM, vendor allow/deny),
  - allow explicit selection (UUID/pci bus id where possible),
  - log selected device and key caps.

### 6.4 Feature/extension gating (MUST)
- Every optional feature/extension must be checked and stored in capability struct.
- All usage sites must branch via capability flags.
- If `VK_KHR_portability_subset` (MoltenVK) is in scope, explicitly document constraints and required subset features.

### 6.5 Memory allocation (MUST)
Blueprint MUST choose allocation strategy:
- VMA (Vulkan Memory Allocator) recommended if allowed, OR
- custom allocator documented with:
  - memory type selection rules
  - suballocation strategy
  - fragmentation mitigation
  - alignment guarantees

Rules:
- Avoid per-frame allocations on hot path.
- Define coherent vs non-coherent mapping rules (flush/invalidate).
- Staging uploads MUST define lifetime and sync boundaries explicitly.

### 6.6 Synchronization and hazards (MUST)
Blueprint MUST choose:
- classic barriers + semaphores/fences, OR
- synchronization2 (Vulkan 1.3 / `VK_KHR_synchronization2`) preferred where available.

Rules:
- Maintain explicit tracking of:
  - pipeline stages/access masks
  - image layouts
  - read/write hazards
- Queue ownership transfers MUST be explicit if multi-queue or multi-family.
- If single-queue model is used, state it explicitly.

### 6.7 Command buffers and pools (MUST)
- Use per-thread command pools (thread-affine).
- Avoid concurrent use of same pool.
- Define per-frame lifecycle (acquire → record → submit → present) including fence/semaphore ownership and reset timing.

### 6.8 Descriptor management (MUST)
Blueprint MUST choose descriptor strategy:
- per-frame descriptor pools with bulk reset, OR
- global pools with suballocation, OR
- descriptor indexing / bindless (requires explicit capability gating).

Rules:
- No per-draw descriptor set allocation.
- Cache descriptor sets or use bindless with stable layouts.
- Layout changes are high-impact; treat as versioned changes with migration notes.

### 6.9 Pipeline management (MUST)
- Pipeline creation MUST not happen on hot path.
- If using `VkPipelineCache`, define:
  - whether persisted to disk,
  - invalidation rules (driver, version, shader changes),
  - portability constraints.

### 6.10 Swapchain and present (if applicable) (MUST)
- Robustly handle:
  - `VK_ERROR_OUT_OF_DATE_KHR`
  - `VK_SUBOPTIMAL_KHR`
- Swapchain recreation MUST be leak-free and fence-guarded.
- Define supported WSI paths:
  - Windows (Win32)
  - Linux (X11/Wayland)
  - macOS via MoltenVK (if applicable) with explicit limitations

### 6.11 Debugging and diagnostics (SHOULD)
- Use `VK_EXT_debug_utils`:
  - object naming
  - debug labels/regions
- Validation layers SHOULD be enabled in dev/CI configs.
- Consider synchronization validation in debug configurations (document overhead).
- RenderDoc integration SHOULD be documented for Vulkan.

### 6.12 Error handling and device lost (MUST)
- Check all `VkResult` returns.
- Map `VkResult` → canonical errors; preserve VkResult as `subcode`.
- Treat `VK_ERROR_DEVICE_LOST` as distinct:
  - explicit recovery or fail-fast policy,
  - define what state can be reconstructed.

### 6.13 Vulkan testing checklist (MUST)
- init smoke: instance/device creation
- command submit smoke (offscreen)
- resource create/destroy stress
- sync correctness smoke (barriers + semaphores)
- swapchain present/resize (if applicable)
- device-lost policy test (as feasible; at minimum simulated failure path)

---

## 7. Metal Backend Rules (Normative)

### 7.1 Backend boundary and Objective‑C++ isolation (MUST)
- Metal backend MUST be isolated in its own target and directory (e.g., `/src/backends/metal/`).
- Objective‑C++ MUST be isolated to `.mm` files in the Metal backend target.
- Core/public headers MUST NOT include Metal or ObjC headers.

### 7.2 Device and capability gating (MUST)
- Default device:
  - `MTLCreateSystemDefaultDevice()` (or equivalent)
- If multiple devices exist, provide deterministic selection via config and log chosen device.
- Gate functionality based on:
  - `supportsFamily:` (Apple GPU family)
  - resource limits (threads per threadgroup, buffer alignment)
  - pixel format support

Store capabilities in an immutable capability struct.

### 7.3 Resource lifetime and ownership (MUST)
- Wrap Metal objects in RAII:
  - `id<MTLDevice>`, `id<MTLCommandQueue>`, `id<MTLBuffer>`, `id<MTLTexture>`, `id<MTLRenderPipelineState>`, etc.
- Wrappers should be movable and avoid exposing ObjC retain/release semantics outside backend.
- Ensure deterministic shutdown and fence/handler completion before destruction.

### 7.4 Command submission model (MUST)
- Define per-frame lifecycle:
  - acquire drawable (if presenting) → encode → commit → present → completion tracking
- Completion handling MUST be explicit (completion handlers for GPU completion).
- Avoid blocking waits on hot path.

### 7.5 Synchronization and hazards (MUST)
Metal is more implicit but you MUST define correctness rules:
- CPU↔GPU visibility for buffers:
  - `storageModeShared` vs `storageModeManaged` (macOS) rules
  - `didModifyRange` and synchronization blits where required
- Resource hazard tracking:
  - define encoder sequencing and resource usage patterns
  - if using heaps/argument buffers, document hazard implications

### 7.6 Buffer updates and staging (MUST)
- Avoid per-draw allocations.
- Use ring buffers with per-frame offsets for dynamic constants.
- Document alignment requirements and padding strategy.

### 7.7 Pipeline and shader strategy (MUST)
Blueprint MUST decide:
- precompiled `.metallib` (recommended for shipping), OR
- runtime compilation (allowed for dev; shipping requires DEC + perf gates)

Rules:
- Cache pipeline states keyed by shader + render state.
- Avoid pipeline creation on hot path.
- If using argument buffers:
  - define support requirements and fallback paths,
  - treat layout changes as versioned changes.

### 7.8 Present/resize (if applicable) (MUST)
- Presentation typically via `CAMetalLayer` + `nextDrawable`.
- Handle drawable acquisition failures gracefully.
- Resize/layer changes MUST be fence/handler safe and leak-free.

### 7.9 Debugging and diagnostics (SHOULD)
- Use Xcode GPU Frame Capture and Metal validation in debug builds.
- Use debug groups/labels for profiling.
- Route validation output through project logging policy.

### 7.10 Error handling (MUST)
- Map Metal failures to canonical errors:
  - pipeline creation failure, command buffer errors, resource allocation failure
- Preserve backend-specific diagnostics (NSError where available) in logs (redacted).
- Define policy for command buffer failure and how it propagates.

### 7.11 Metal testing checklist (MUST)
- init smoke: device + command queue creation
- resource create/destroy stress
- buffer update correctness (CPU→GPU visibility)
- offscreen render pass smoke (CI-friendly; recommended)
- present/resize if applicable

---

## 8. Cross-platform CI strategy for GPU (Normative guidance)

### 8.1 CI reality constraints
GPU runners may not exist. The blueprint MUST explicitly choose one of:
- **Full GPU CI**: GPU hardware runners per backend (ideal).
- **Partial CI**: build + headless init tests; deeper tests run on dedicated nightly runners.
- **Stub gating**: build-only + strict failing-fast gate that blocks releases without manual verification on required hardware.

### 8.2 Minimum CI requirements (MUST)
Even without GPU hardware, CI MUST:
- compile the backend on its supported OS (Vulkan Linux/Windows; DX12 Windows; Metal macOS),
- run at least an init smoke test where possible (headless device creation),
- run validation/lint/format gates as normal.

### 8.3 Headless/offscreen recommendations
- Vulkan: headless surface extension or offscreen rendering (no WSI); validate command submission.
- DX12: WARP fallback may be used for smoke (DEC required; document correctness limitations).
- Metal: offscreen texture render target (no CAMetalLayer) for smoke tests.

---

## 9. “Must-not-do” pitfalls (Common) (Normative)

The following are forbidden unless DEC + enforcement:
- Creating PSOs/pipelines per draw call.
- Allocating descriptors/descriptor sets per draw call.
- Blocking CPU waits for GPU completion on hot path.
- Using global locks around hot-path submission or binding.
- Leaking backend headers into public headers.
- Assuming a feature is present without capability gating.
- Logging massive shader/resource dumps in production builds.

---

## 10. Quick common checklist (MUST for blueprint review)

- [ ] Backend isolated in dedicated target + folder
- [ ] No GPU headers leak into public/core headers
- [ ] Deterministic device selection with capability struct
- [ ] Explicit ownership and fence-guarded reclamation
- [ ] Synchronization/hazard strategy documented and implemented
- [ ] Descriptor/binding strategy avoids per-draw allocation
- [ ] Pipeline/PSO creation avoided on hot path; caching exists
- [ ] Error mapping + device lost policy defined
- [ ] Debug tooling integrated (validation layers, markers)
- [ ] Tests: init smoke, resource stress, sync smoke; present/resize if applicable
- [ ] CI plan chosen and enforced (full/partial/stub + failing-fast gates)

---

## 11. Policy changes
Changes to this file MUST be introduced via CR and MUST include:
- updated enforcement expectations (tests/gates),
- migration notes for affected GPU abstraction boundaries,
- CI strategy updates if backend coverage changes.
