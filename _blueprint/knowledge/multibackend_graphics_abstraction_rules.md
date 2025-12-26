<!-- _blueprint/knowledge/multibackend_graphics_abstraction_rules.md -->

# Multi-Backend Graphics Abstraction Rules (Vulkan/Metal/DX12) — Normative
This document standardizes how to design a **portable graphics abstraction layer** that can host multiple backends:
- Vulkan ✅
- Metal ✅
- DirectX 12 ✅

It focuses on clean boundaries, predictable performance, and long-term evolvability (SemVer + migrations).

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Goals
- Keep the **core API backend-agnostic** (no Vk/MTL/D3D types in core unless explicitly designed).
- Enable backend-specific optimizations without infecting the whole codebase.
- Make resource lifetime, synchronization, and error behavior consistent across backends.
- Support future features (v2.0+) via versioned interfaces and capability gating.

---

## 2) Layering & Targets (Mandatory)
Recommended target topology:
- `<proj>` (core): public API + backend-agnostic abstractions + selection/factory
- `<proj>_vk`, `<proj>_mtl`, `<proj>_dx12`: backend targets
- `<proj>_shader`: optional shader/asset pipeline tools (if needed)

**Rules:**
- Core must not include backend headers.
- Backends depend on core interfaces only.
- Backend-specific files live under `/src/backends/{vulkan,metal,dx12}/`.

---

## 3) Capability Model (Mandatory)
Core must define a backend capability struct (stable, versionable):
- device name/vendor
- feature flags (e.g., bindless-like support, timestamp queries, MSAA limits)
- resource limits (max textures, buffer alignment, descriptor heap sizes, etc.)
- shader model/feature set indicators (abstracted)

**Rules:**
- Every optional feature must be gated by capabilities.
- The chosen backend and capabilities must be logged (per [REQ-02]).

---

## 4) Public API Shape (Recommended)
Choose one of these surface strategies (record in [DEC-XX], define in [API-02]):
### Strategy A: Thin “Engine API” (recommended)
Expose:
- `Device` / `Context`
- `CommandList` (recording)
- `Buffer`, `Texture`, `Sampler`, `Pipeline`, `Fence/Semaphore` (as needed)
Hide:
- raw backend handles by default

### Strategy B: Handle-oriented C++ API (portable “C-ish”)
Expose opaque handles and POD descriptors; implement behind the scenes.
Best for ABI stability.

**Rule:** Avoid exposing backend types in public headers unless you intentionally allow “interop”.

---

## 5) Interop Policy (Optional)
If you allow native interop:
- Provide *explicit* functions to retrieve native handles:
  - `get_native_handle(Buffer)` returning tagged union or void* + enum
- Document lifetime: the handle is valid only while the wrapper object lives.
- Interop must be a separate optional header/module to avoid contaminating normal users.

---

## 6) Resource Model (Mandatory)
### 6.1 Descriptors/Descriptors structs
Define POD “create descriptor” structs for all resources:
- `BufferDesc`, `TextureDesc`, `SamplerDesc`, `PipelineDesc`, etc.
Include:
- usage flags
- format
- size/extent
- initial state/usage intent (abstracted)
- debug name (optional in release)

**Rules:**
- Descriptors are versionable: append-only fields or a `version/size` header if ABI stability matters.
- Avoid hidden allocations in resource creation unless documented.

### 6.2 Lifetime and ownership
- Resources use RAII in C++.
- Destruction must be safe relative to GPU execution:
  - either deferred destruction queues (recommended)
  - or explicit “device idle / fence” requirements documented

**Rule:** The Blueprint must specify the destruction safety model and tests for it.

---

## 7) Synchronization Semantics (Mandatory)
Portable sync is the #1 source of bugs. Define an explicit abstraction:

### 7.1 Resource states (portable)
Define abstract states/usages:
- `CopySrc`, `CopyDst`
- `Vertex/Index`, `Uniform`, `Storage`
- `RenderTarget`, `DepthStencil`
- `ShaderRead`, `ShaderWrite`
- `Present` (if applicable)

Backends map these to:
- Vulkan: layouts + access/stages
- DX12: D3D12_RESOURCE_STATES
- Metal: encoder/hazard model + storage mode visibility rules

### 7.2 Barriers
Provide a portable `Barrier` type:
- resource
- before/after usage
- subresource range (optional)
- queue scope (optional)

**Rules:**
- Core defines correctness rules; backend implements mapping.
- If using a simplified model (single-queue), state it explicitly.

---

## 8) Command Recording Abstraction (Recommended)
### 8.1 Command list model
- Provide `CommandList` that records:
  - copy ops
  - resource transitions (or implicit via usage tracking)
  - compute/graphics passes
  - debug markers (optional)

### 8.2 Pass-based API (recommended)
Use pass encoders:
- `begin_render_pass(...)` → draw calls → `end_render_pass()`
- `begin_compute_pass(...)` → dispatch → `end_compute_pass()`

Backends map to:
- Vulkan: command buffer + render pass/dynamic rendering
- Metal: render/compute encoders
- DX12: command list + OM/CS setup

**Rule:** Avoid overly “lowest-common-denominator” APIs that prevent backend strengths.

---

## 9) Descriptor/Binding Model (Mandatory decision)
You must decide one binding approach and document it:
- **Bind groups / descriptor sets** style (portable, recommended)
- **Root signature / argument buffers** mapping layer (advanced)

Suggested portable model:
- `BindGroupLayout` + `BindGroup`
- `PipelineLayout` references bind group layouts
- dynamic offsets supported where applicable

Backends map to:
- Vulkan: descriptor set layouts/sets
- DX12: root signature + descriptor tables
- Metal: argument buffers (optional) or direct resource binding

**Rule:** Keep layout stable; treat layout changes as potentially breaking (tie to SemVer and MIGRATION).

---

## 10) Shader Strategy (Mandatory decision)
Blueprint must choose:
- offline compilation pipeline (recommended for shipping)
- runtime compilation only for dev tools

Recommended portable approach:
- Use an intermediate representation/toolchain (project-dependent) OR
- Maintain per-backend shader outputs:
  - Vulkan: SPIR-V
  - DX12: DXIL
  - Metal: metallib

**Rule:** Shader interface (resources/bindings) must be versioned; provide migration notes for breaking changes.

---

## 11) Error Model (Mandatory)
- All backend errors map to canonical errors (see `error_handling_playbook.md`).
- Preserve backend-specific subcodes:
  - Vulkan: VkResult
  - DX12: HRESULT
  - Metal: NSError codes/messages (as available)

**Rule:** Device-lost / device-removed must have an explicit policy (recover or fatal), documented and tested where feasible.

---

## 12) Threading Model (Mandatory)
Blueprint must define:
- whether command recording is multi-threaded
- submission threading model
- cache locking strategy (pipelines/descriptors)

Rules:
- Prefer per-thread command contexts.
- Avoid global locks on hot paths.
- Backend objects with thread affinity must be explicit (e.g., command pools/allocators).

---

## 13) Testing Requirements (Mandatory)
Blueprint must include [TEST-XX] entries for:
- backend selection + capability reporting smoke test
- resource create/destroy stress test (leaks, deferred destruction correctness)
- barrier/state correctness smoke test (render + compute + copy)
- swapchain/drawable + resize test (if present)
- device lost/removed handling (as feasible)
- cross-backend parity test for a minimal feature subset (recommended)

---

## 14) Evolution & Versioning Guidance
- Define a stable abstraction “v1” and avoid large rewrites.
- Add features via capability flags + new optional methods.
- Breaking changes:
  - require MAJOR bump ([VER-01]) and MIGRATION.md updates ([VER-05]).
- Deprecate API before removal ([VER-04]).

---

## 15) Quick Checklist
- [ ] Backend targets isolated and core is backend-agnostic
- [ ] Capability struct gates optional features
- [ ] Portable resource/state/barrier model defined and correctly mapped
- [ ] Binding model chosen and stable
- [ ] Shader pipeline choice made and versioned
- [ ] Error mapping preserves backend subcodes
- [ ] Threading model explicit; avoids global hot locks
- [ ] Tests cover smoke/leaks/barriers/resize/parity
