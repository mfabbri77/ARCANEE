# GPU APIs Backend Rules (Normative)
This document defines best-practice rules for implementing **GPU backends** inside a multi-backend native C++ project.
It consolidates backend-specific rules for **DirectX 12**, **Vulkan**, and **Metal**, and is structured to allow adding
additional GPU APIs in the future (e.g., OpenGL, WebGPU, CUDA interop, etc.) as new sections.

> **Precedence (within the overall system):** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

## How to extend this file (for new GPU APIs)
- Add a new top-level section: `## <API Name> Backend Rules (Normative)`
- Keep the same internal structure as existing backends:
  - Boundary / leakage rules
  - Device selection + capability gating
  - Resource lifetime + ownership
  - Synchronization / hazards
  - Binding/descriptor model
  - Pipeline + shader strategy
  - Present/swapchain (if applicable)
  - Debug/diagnostics
  - Error handling
  - Threading model
  - Testing requirements
  - Quick checklist
- If you introduce cross-cutting rules that apply to *all* GPU backends, put them in **Common GPU Backend Rules** below.

---

## Common GPU Backend Rules (Applies to all backends)
### A) Backend isolation and header hygiene (Mandatory)
- Each backend MUST live in its own dedicated target and directory (e.g., `<proj>_vk`, `<proj>_dx12`, `<proj>_mtl`).
- The core MUST depend on backends only via an interface (factory/registry). Backends MUST NOT leak platform/GPU headers into
  core/public headers unless explicitly part of the public API.

### B) Capability gating (Mandatory)
- Backend capabilities MUST be queried at startup, captured in a capability struct, and used to gate feature usage.
- Optional features MUST never be assumed present based on “common hardware” heuristics.

### C) Hot-path discipline (Mandatory)
- Avoid allocations and expensive object creation on the render hot path (descriptor allocations per draw, pipeline creation per draw, etc.).
- Prefer pooling/caching and per-frame contexts.

### D) Error handling (Mandatory)
- Backend-specific error codes MUST be mapped to canonical project errors per `error_handling_playbook.md`.
- Preserve the raw backend error code as a `subcode` or equivalent field for diagnostics.

### E) Diagnostics and TEMP-DBG (Mandatory)
- Integrate with project logging/observability policy.
- Any ad-hoc debug prints MUST follow `temp_dbg_policy.md`.

### F) Testing baseline (Mandatory)
Blueprints MUST define tests for:
- init smoke (device creation + minimal submission)
- resource creation/destruction stress (leak detection)
- synchronization correctness smoke (as applicable)
- present/resize (if applicable)
- device lost/removed handling strategy (as feasible)
- a CI-friendly headless/offscreen mode is RECOMMENDED.

---

## DirectX 12 Backend Rules (Normative)

### 1) Backend Boundary (Mandatory)
- DX12 code must live under a dedicated backend target (e.g., `<proj>_dx12`) and folder (e.g., `/src/backends/dx12/`).
- Core must depend on backend via an interface (factory/registry). Backend must not leak Windows/DX headers into core/public headers unless explicitly part of the public API.

**Rule:** Public headers under `/include/<proj>/` must not include `d3d12.h`, `dxgi.h`, `windows.h` unless the SDK explicitly exposes DX objects.

### 2) Device & Adapter Selection
#### 2.1 DXGI factory & adapter enumeration
- Enumerate adapters via DXGI.
- Prefer high-performance GPU (discrete) unless overridden by configuration.
- Support explicit adapter selection by LUID/index for reproducibility.

#### 2.2 Feature level and caps
- Choose minimum `D3D_FEATURE_LEVEL` required by the project and document it.
- Query key caps:
  - resource binding tier
  - descriptor heap sizes
  - shader model requirements
  - optional features (mesh shaders, sampler feedback, etc.) only if needed

**Rule:** Capabilities must be captured in a capability struct and gate feature usage.

### 3) COM & Lifetime Management (Mandatory)
- Use RAII for COM objects (`ComPtr` or equivalent).
- Do not manually `Release()` except in rare, justified cases.
- Ensure deterministic shutdown order: command queues → resources → device/factory.

**Rule:** Resource lifetime must be validated with leak reporting in debug builds.

### 4) Synchronization Model (Mandatory)
#### 4.1 Fences
- Use fences for GPU/CPU synchronization.
- Avoid CPU waits on hot path; batch work and use frame-latency buffers.

#### 4.2 Multi-queue usage
If using multiple queues (graphics/compute/copy):
- define ownership model and how resources move between queues
- use fences to synchronize between queues

**Rule:** Cross-queue sync must be explicit and documented.

### 5) Resource States & Barriers (Mandatory)
#### 5.1 State tracking
- Implement explicit per-resource state tracking.
- Track at least:
  - current `D3D12_RESOURCE_STATES`
  - whether state is per-subresource or whole resource (choose one model and justify)
  - pending transitions for batching

#### 5.2 Barrier batching
- Batch barriers to reduce overhead.
- Prefer correct minimal transitions; avoid “transition to COMMON everywhere” patterns.

**Rule:** Every resource usage must be preceded by a correct state transition (or a proven-safe implicit state).

### 6) Descriptor Heaps & Binding Strategy (Mandatory)
Blueprint must choose a descriptor strategy:
- CPU-only staging heaps + GPU-visible ring heap
- Global GPU-visible heap with suballocation
- Per-frame descriptor heap resets

Rules:
- Avoid allocating descriptors per draw call.
- Prefer descriptor caching and stable root signatures.

**Root signature rule:** keep root signatures stable; changes can be breaking for pipeline caches and shader compatibility.

### 7) Pipeline State & Shader Strategy (Mandatory)
#### 7.1 Shader compilation
Blueprint must decide:
- offline compilation to DXIL (recommended for shipping)
- runtime compilation only for dev tools (if needed)

#### 7.2 PSO caching
- Cache PSOs keyed by shader + render state.
- Avoid PSO creation on hot path.
- Consider disk cache for PSOs (optional) and define invalidation rules.

### 8) Command Lists & Allocators
- Use per-thread command allocators and command lists (or per-frame-per-thread).
- Reset allocators only after GPU has finished using them (tracked via fences).
- Define frame contexts and reuse resources.

**Rule:** Command allocator reset must be fence-guarded.

### 9) Swap Chain & Present (If applicable)
- Use DXGI swap chain appropriate for the app (flip model recommended).
- Handle resize robustly:
  - wait for GPU idle or safe point
  - release back buffers
  - resize buffers
  - recreate RTVs

**Rule:** Resize must be leak-free and validated in debug builds.

### 10) Debugging & Diagnostics (Recommended)
#### 10.1 Debug layer
- Enable D3D12 debug layer in dev/ci builds.
- Enable GPU-based validation optionally (slow; use when diagnosing).

#### 10.2 DRED
- Integrate DRED (Device Removed Extended Data) for device removed diagnostics.
- On device removed, capture and log DRED data if possible.

#### 10.3 Naming
- Name resources and command lists in debug builds for easier debugging.

**TEMP-DBG rule:** Any ad-hoc debug prints must follow `temp_dbg_policy.md`.

### 11) Error Handling (Mandatory)
- Map `HRESULT` to canonical errors per `error_handling_playbook.md`.
- Preserve raw `HRESULT` as `subcode`.
- Handle device removed/reset (`DXGI_ERROR_DEVICE_REMOVED`, `DXGI_ERROR_DEVICE_RESET`) with an explicit recovery policy:
  - recreate device/resources OR fatal exit
  - document in the Blueprint.

### 12) Threading Model (Mandatory)
Blueprint must define:
- which threads record command lists
- how submissions are synchronized
- locking strategy for shared caches (PSO cache, descriptor allocators)

Rules:
- Avoid global locks on hot paths.
- Prefer per-thread recording contexts and lock-free submission queues if needed.

### 13) Testing Requirements (Mandatory)
Define [TEST-XX] entries in Blueprint for:
- init smoke: adapter/device creation
- command queue + fence correctness test
- resource create/destroy stress (leak detection)
- barrier correctness smoke (render/compute pass validation)
- swapchain present + resize stress (if applicable)
- device removed handling strategy test (as feasible)

Recommended:
- headless/offscreen path for CI-friendly rendering tests.

### 14) Quick Checklist
- [ ] DX12 backend isolated in `<proj>_dx12` target and folder
- [ ] deterministic adapter selection and capability gating
- [ ] explicit resource state tracking + barrier batching
- [ ] descriptor heap strategy avoids per-draw allocations
- [ ] PSO caching and offline shader compilation policy defined
- [ ] debug layer + DRED integrated for diagnostics (dev builds)
- [ ] canonical error mapping with HRESULT subcodes
- [ ] tests cover init/leaks/barriers/resize/device removed handling

---

## Vulkan Backend Rules (Normative)

### 1) Backend Boundary (Mandatory)
- Vulkan code must live under a dedicated backend target (e.g., `<proj>_vk`) and directory (e.g., `/src/backends/vulkan/`).
- Core must depend on backend via an interface (factory/registry). Backend must not leak Vulkan types into core/public headers unless explicitly part of the public API.

**Rule:** Public headers under `/include/<proj>/` should not include `<vulkan/vulkan.h>` unless the SDK explicitly exposes Vulkan objects.

### 2) Instance/Device Creation Policy
#### 2.1 Loader & dispatch
- Use volk or the platform loader directly, but **codify one approach** in the Blueprint.
- Ensure function pointers are loaded once and shared safely.

#### 2.2 Instance extensions/layers
- Separate “required” vs “optional” extensions.
- Validation layers are enabled only in dev/ci configs or via runtime flag.

Recommended minimum instance extensions (platform-dependent):
- `VK_KHR_surface` + platform surface extension (Win32/Xcb/Xlib/Wayland/Metal via MoltenVK if used)
- `VK_EXT_debug_utils` (debug builds)

#### 2.3 Physical device selection
Selection must be deterministic and documented:
- Prefer discrete GPUs (unless overridden).
- Require minimum Vulkan version and feature set.
- Prefer required extensions and features; reject devices that do not support them.

**Rule:** Device selection must be configurable and log the chosen device and feature set.

### 3) Feature/Extension Gating (Mandatory)
- Every optional feature/extension must be checked and stored in a capability struct.
- All usage sites must branch via capability flags.

**Rule:** Never assume an extension is present because “most GPUs have it”.

### 4) Memory Allocation (Recommended)
#### 4.1 Allocator choice
- Prefer Vulkan Memory Allocator (VMA) unless explicitly forbidden.
- If not using VMA, document your allocator strategy (pools/arenas) and memory type selection rules.

#### 4.2 Allocation rules
- Avoid per-frame allocations on the hot path.
- Prefer pooling and suballocation.
- Use dedicated allocations only when justified (very large resources, aliasing constraints, etc.).

#### 4.3 Mapping rules
- For frequently updated buffers, prefer persistently mapped staging or host-visible memory (depending on performance and platform).
- Explicitly define coherency handling (flush/invalidate) when memory is non-coherent.

### 5) Synchronization & Hazards (Mandatory)
#### 5.1 Synchronization model
Blueprint must choose one:
- Classic barriers + semaphores/fences
- Vulkan 1.3 synchronization2 (`VK_KHR_synchronization2`) preferred where available

#### 5.2 Resource state tracking
- Prefer an explicit state tracker (per resource) to avoid incorrect barriers.
- Track:
  - last access type (read/write)
  - pipeline stages
  - access masks
  - image layouts (for images)

#### 5.3 Queue ownership
- If multiple queues are used (graphics/compute/transfer), define:
  - when ownership transfers occur
  - queue family indices and transitions
- If only one graphics queue is used, explicitly state “single-queue model” (simpler, often fine).

**Rule:** Any cross-queue resource use must have explicit synchronization and ownership correctness.

### 6) Command Buffers & Pools
- Use per-thread command pools (or per-frame-per-thread) to avoid contention.
- Prefer resettable pools and reuse command buffers.
- Clearly define “frame lifecycle”:
  - acquire image → record → submit → present
  - fences/semaphores timeline

**Rule:** Command pool ownership is thread-affine; never reset/allocate from the same pool concurrently.

### 7) Descriptor Management (Recommended)
Blueprint must choose a descriptor strategy:
- Descriptor pool per frame with bulk reset
- Global pools + suballocation
- Descriptor indexing (`VK_EXT_descriptor_indexing`) if required/available

Rules:
- Avoid allocating descriptors per draw call.
- Prefer descriptor set caching or bindless (if supported and worth it).
- Keep layout stable across versions where possible; treat layout changes as potential breaking changes for pipelines.

### 8) Pipeline Management
#### 8.1 Pipeline cache
- Use `VkPipelineCache` and persist cache to disk if beneficial (optional).
- Define cache invalidation rules (driver changes, versioning).

#### 8.2 Pipeline creation
- Avoid pipeline creation on the render hot path.
- Precompile or async-compile pipelines where appropriate; define fallback behavior.

### 9) Swapchain & Present (If applicable)
- Handle `VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` robustly.
- Recreate swapchain on resize and surface changes.
- Ensure resources dependent on swapchain images are recreated consistently.

**Rule:** Swapchain recreation must be safe and leak-free, validated by ASan/valgrind where feasible.

### 10) Debugging & Diagnostics (Recommended)
- Use `VK_EXT_debug_utils` with labeled regions and object names in debug builds.
- Integrate with the project logging policy (no printf spam).
- Provide toggles for:
  - validation layers
  - GPU-assisted validation (if used)
  - synchronization validation (helpful)

**TEMP-DBG rule:** Any ad-hoc Vulkan debug prints must follow `temp_dbg_policy.md`.

### 11) Error Handling (Mandatory)
- Map `VkResult` to canonical errors per `error_handling_playbook.md`.
- Preserve the raw `VkResult` as `subcode`.
- Treat `VK_ERROR_DEVICE_LOST` as a distinct, high-severity error with explicit recovery policy:
  - either “fatal and recreate device” (if supported) or “fatal exit”
  - document the choice in the Blueprint.

### 12) Threading Model (Mandatory)
Blueprint must state:
- Which Vulkan objects are accessed from which threads.
- Whether command recording is multi-threaded.
- Locking strategy for shared caches (pipeline cache, descriptor caches).

Rules:
- Avoid global locks on hot paths.
- Prefer per-thread resources and lock-free queues for submission (if needed).

### 13) Cross-Platform Notes
- Windows: Win32 surface; WSI nuances.
- Linux: X11 vs Wayland surfaces; document supported WSI paths.
- macOS: Vulkan typically via MoltenVK; document limitations and required extensions/features.

**Rule:** If targeting macOS with Vulkan, explicitly document MoltenVK constraints and the feature subset you rely on.

### 14) Testing Requirements (Mandatory)
Define [TEST-XX] entries in Blueprint for:
- smoke init test: instance/device creation
- swapchain create/draw/present (if applicable)
- resource creation and destruction stress (leak detection)
- synchronization validation runs (dev/ci)
- device-lost simulation strategy (if feasible)

Recommended:
- a headless mode for CI (no window system) when possible.

### 15) Quick Checklist
- [ ] Vulkan backend isolated in `<proj>_vk` target and folder
- [ ] deterministic device selection with capability gating
- [ ] allocator strategy defined (VMA or custom)
- [ ] synchronization model and resource state tracking defined
- [ ] descriptor and pipeline strategies avoid hot-path allocations/creation
- [ ] robust swapchain recreation (if applicable)
- [ ] VkResult mapped to canonical errors (subcode preserved)
- [ ] debug utils integrated in debug builds
- [ ] tests cover init/smoke/leaks/sync validation

---

## Metal Backend Rules (Normative)

### 1) Backend Boundary & Objective-C++ Isolation (Mandatory)
- Metal code must live under a dedicated backend target (e.g., `<proj>_mtl`) and folder (e.g., `/src/backends/metal/`).
- All Objective-C++ must be isolated to `.mm` files inside the Metal backend target.
- Core and other backends must not include `<Metal/Metal.h>` or any Obj-C headers.

**Rule:** Public headers under `/include/<proj>/` must not expose Metal types unless the SDK explicitly designs for Metal-native integration.

### 2) Device & Feature Set Policy
#### 2.1 Device selection
- Use `MTLCreateSystemDefaultDevice()` by default.
- If multiple devices exist, allow selection via config and log the chosen device name.

#### 2.2 Feature gating
- Gate functionality based on:
  - `supportsFamily:` / feature sets
  - resource limits (threads per threadgroup, buffer alignment)
  - pixel format support
- Store capabilities in a backend capability struct.

**Rule:** No assumptions; capability checks must drive code paths.

### 3) Resource Lifetime & Ownership (Mandatory)
- Use RAII wrappers for:
  - `id<MTLDevice>`, `id<MTLCommandQueue>`, `id<MTLBuffer>`, `id<MTLTexture>`, `id<MTLRenderPipelineState>`, etc.
- Ensure wrappers are movable, non-copyable unless explicitly needed.
- Do not leak Objective-C reference counting semantics into core; keep it encapsulated.

**Rule:** All resources created must have deterministic destruction and be stress-tested for leaks.

### 4) Command Submission Model
#### 4.1 Command queue usage
- Typically use a single `MTLCommandQueue` per device.
- If multiple queues are used, document why and how they are synchronized.

#### 4.2 Frame lifecycle
Define a clear per-frame lifecycle, e.g.:
- begin frame → acquire drawable (if present) → encode → commit → present → completion handling

#### 4.3 Completion handling
- Prefer completion handlers for async tracking.
- For CPU/GPU sync, avoid blocking waits on hot paths.

**Rule:** The Blueprint must define how you wait for GPU work (if ever) and where (e.g., shutdown, resize, resource reclaim).

### 5) Synchronization & Hazards
Metal is largely implicit but you must still enforce correctness:
- Manage resource hazards by correct encoder usage and resource options.
- For shared CPU/GPU buffers:
  - decide between `storageModeShared` and `storageModeManaged` (macOS)
  - define flush/invalidate steps when required (`didModifyRange`, blit sync)

**Rule:** The Blueprint must define how CPU-written data becomes visible to GPU and vice versa.

### 6) Buffer Updates & Staging (Recommended)
- For frequent updates:
  - ring buffers in `storageModeShared` (Apple silicon often performs well)
  - double/triple buffering by frame
- Avoid per-draw buffer allocations.
- Track alignment requirements (e.g., constant buffer alignment) and pad accordingly.

### 7) Pipeline & Shader Strategy (Mandatory)
#### 7.1 Shader compilation
Blueprint must decide:
- precompiled `.metallib` bundled with app, OR
- runtime compilation from MSL sources (discouraged for shipping)

#### 7.2 Pipeline state caching
- Cache pipeline states keyed by shader + render state.
- Avoid pipeline creation on the hot path.

#### 7.3 Argument buffers (Optional)
If using argument buffers:
- define support requirements and fallback paths.
- treat layout changes as potentially breaking for pipeline caches.

### 8) Swapchain/Present Equivalent (If applicable)
If rendering to a window:
- typically via `CAMetalLayer` and `nextDrawable`.
- handle drawable acquisition failures gracefully.
- handle resize and layer changes safely.

**Rule:** Resize and drawable recreation must be leak-free and validated by tests.

### 9) Error Handling & Diagnostics (Mandatory)
- Map Metal failures to canonical errors per `error_handling_playbook.md`.
- Preserve backend-specific codes/messages where possible.
- Use the project logger (no printf spam).

Diagnostics:
- Use `MTLDebugDevice` features when available (debug builds).
- Capture validation errors where possible and route to logs.

### 10) Threading Model (Mandatory)
Blueprint must define:
- which threads create resources
- which threads encode command buffers
- whether encoding is multi-threaded

Rules:
- Avoid shared mutable state without clear locking.
- Use per-thread command encoders or per-frame contexts.

### 11) Performance Guidelines (Practical)
- Minimize CPU overhead per draw:
  - batch state changes
  - reuse command buffers where feasible
- Use resource heaps where beneficial (advanced; optional).
- Prefer pipeline and resource reuse; avoid frequent churn.

### 12) Cross-Version Stability
- Treat shader interface changes as versioned changes.
- Keep binding layouts stable when possible.
- If breaking changes are unavoidable, tie them to SemVer and provide MIGRATION notes.

### 13) Testing Requirements (Mandatory)
Define [TEST-XX] entries in Blueprint for:
- init smoke: device + command queue creation
- resource create/destroy stress (leak detection)
- buffer update correctness test (CPU→GPU visibility)
- render pass smoke (headless or offscreen if possible)
- resize/layer change scenario (if applicable)

Recommended:
- offscreen rendering path for CI-friendly tests.

### 14) Quick Checklist
- [ ] Metal backend isolated in `<proj>_mtl` target with `.mm` files
- [ ] capability gating via feature sets/families
- [ ] explicit CPU↔GPU visibility rules for shared/managed storage
- [ ] pipeline caching and shader strategy (metallib) defined
- [ ] robust drawable acquisition/resize handling (if applicable)
- [ ] canonical error mapping and structured logging
- [ ] tests cover init/leaks/buffer correctness/offscreen smoke
