<!-- multibackend_graphics_abstraction_rules.md -->

# Multi-backend Graphics Abstraction Rules (Normative)

This document defines the **architecture, layering, and contract rules** for implementing a graphics/GPU abstraction that supports multiple backends (Vulkan, Metal, DirectX 12) in a C++ project governed by the DET CDD blueprint system.

It is **normative** whenever more than one backend is supported or when the blueprint intends to keep GPU logic portable across platforms.

---

## 1. Goals

- Provide a stable backend-agnostic interface for GPU operations.
- Isolate backend-specific code and headers.
- Allow controlled exposure of native handles only when explicitly desired.
- Enable deterministic behavior and strong testing/CI gates.
- Minimize performance overhead from abstraction.

---

## 2. Mandatory structure and layering

### 2.1 Layering (MUST)

The GPU system MUST be split into these layers:

1. **Public API layer** (optional)
   - Exposed to consumers (SDK/users).
   - MUST be backend-agnostic unless DEC explicitly exposes native handles.

2. **Core GPU abstraction layer** (required)
   - Defines backend-independent interfaces and data structures.
   - Contains resource descriptors, pipeline descriptions, command recording model.
   - No platform or backend headers.

3. **Backend implementations** (required)
   - One per backend.
   - Includes native headers and platform-specific code.
   - Implements core interfaces.

4. **Platform surface/window integration** (optional, but common)
   - Window handles, swapchain surface creation (Win32, Cocoa, X11/Wayland).
   - MUST be isolated from core if cross-platform.

### 2.2 Dependency direction (MUST)

Dependency direction MUST be one-way:

`Public API → Core → Backend → Platform`

Forbidden:
- Core including backend headers
- Public API including backend headers by default
- Backends depending on application code

---

## 3. Interface design rules (MUST)

### 3.1 Minimal stable interfaces

Core interfaces MUST be:
- minimal,
- explicit about ownership, lifetime, and threading,
- designed to avoid virtual-call overhead on hot path where possible.

Common approaches:
- pure virtual interfaces for high-level objects (device, queue)
- value-type command buffers encoded into backend-specific command lists
- handle-based APIs (opaque integer handles) for high performance

A DEC MUST choose the interface approach and justify performance impact.

### 3.2 Handle vs object model

The abstraction MUST choose one of:

- **Object model**:
  - `GpuDevice`, `GpuBuffer`, etc. as RAII objects.
  - Backends implement with pImpl or derived types.
- **Handle model**:
  - core defines opaque handles (`GpuBufferHandle`).
  - core maintains tables; backends manage native objects.

Rules:
- handle model tends to be more deterministic and cache-friendly.
- object model can be simpler but may incur virtual dispatch costs.

Choice requires DEC + perf gate if hotpath sensitive.

### 3.3 Descriptor/pipeline descriptors are backend-agnostic

Core MUST define backend-agnostic descriptors for:
- buffers/textures
- samplers
- render pipelines / compute pipelines
- resource bindings

Backends translate descriptors to native objects.

---

## 4. Resource binding model (MUST)

### 4.1 Unified binding layout

Core MUST define a unified binding layout abstraction such that:
- Vulkan descriptor sets / layouts
- DX12 root signatures + descriptor heaps
- Metal argument buffers or resource slots

can map consistently.

Rules:
- The binding model MUST be stable and versioned.
- Changes to binding layout are high-impact and require:
  - DEC
  - tests
  - migration notes
  - SemVer bump if public

### 4.2 Bindless (optional)

If bindless is used (descriptor indexing / tier 2/3 / argument buffers):
- Must be capability-gated.
- Must have fallback or explicitly declare “bindless required” with platform constraints.

---

## 5. Command recording and submission (MUST)

### 5.1 Command model choice

Core MUST define a command model:
- immediate mode (record and submit directly)
- deferred recorded command buffers/encoders

Choice MUST include:
- threading: where recording occurs
- how submission is synchronized
- reuse strategy (command pools/allocators)

### 5.2 Hotpath constraints

The command model MUST avoid:
- per-command heap allocations,
- global locks on command recording paths,
- backend object creation during command encoding.

---

## 6. Synchronization and resource state tracking (MUST)

Core MUST define:
- resource state enumeration (common subset)
- barrier model (transition, acquire/release)
- queue ownership model (single vs multi-queue)

Backends implement:
- DX12 state transitions explicitly
- Vulkan layouts and pipeline barriers
- Metal resource usage/hazard behavior

If the core model cannot express a backend’s requirements:
- add a backend extension point via DEC, not ad-hoc hacks.

---

## 7. Shader strategy (MUST)

Core MUST define:
- shader stage model (vs/ps/cs etc.)
- input/output reflection info required (binding slots, push constants, root constants)
- pipeline cache keys and hashing rules

If multiple backends are supported:
- choose a cross-backend shader strategy (DEC):
  - HLSL → DXIL (DX12) and SPIR-V (Vulkan) and MSL (Metal via translation)
  - GLSL → SPIR-V (Vulkan) and translated to others
  - Offline compilation preferred for determinism

---

## 8. Native handle exposure (forbidden by default)

### 8.1 Default rule

Core and public APIs MUST NOT expose native backend objects by default:
- VkDevice, VkImage, ID3D12Resource*, id<MTLBuffer>, etc.

### 8.2 Allowed via explicit DEC

If native handle exposure is required:
- provide a clearly separated extension API:
  - `getNativeHandle()` returning opaque tagged union
- guard by compile-time feature flag
- document ABI implications (cpp_api_abi_rules.md)
- add tests ensuring correct type/ownership

---

## 9. Error model (MUST)

Core MUST define a backend-agnostic error model:
- stable error codes/categories
- optional backend subcode storage
- device-lost representation

Backends translate native errors to core errors.

---

## 10. Testing and CI (MUST)

### 10.1 Minimum tests

For each backend supported:
- init smoke test (headless where possible)
- command submission smoke
- resource creation/destruction stress
- sync correctness smoke
- present/resize test (if swapchain supported)

Core-level tests:
- descriptor translation correctness tests
- pipeline cache key stability tests
- determinism tests (if required)

### 10.2 CI coverage strategy

- Build each backend on its supported OS.
- If hardware CI is unavailable:
  - run headless tests where possible
  - add failing-fast gates requiring manual hardware verification before release

---

## 11. Performance constraints (conditional MUST)

If performance budgets are declared:
- the abstraction MUST define:
  - acceptable overhead (CPU time per command encoding, allocations)
  - benchmark(s) measuring overhead vs backend-native path if possible
- CI SHOULD run perf smoke tests or dedicated perf runners.

---

## 12. Policy changes

Changes to these rules MUST be introduced via CR and MUST include:
- migration notes for interface changes,
- updated tests and gates,
- SemVer impact rationale.
