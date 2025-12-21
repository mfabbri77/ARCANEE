# Chapter 7 — 3D Scene API and Rendering Model (Diligent-based) (ARCANEE v0.1)

## 7.1 Overview

ARCANEE v0.1 exposes a 3D rendering and scene management API to cartridges under the namespace `gfx3d.*`. The API provides a **modern, PBR-capable** 3D pipeline while preserving ARCANEE’s core guarantees:

* deterministic simulation (Chapter 4)
* sandboxed asset access (Chapter 3)
* CBUF-based rendering and present modes (Chapter 5)

The 3D system is implemented on top of **DiligentEngine** and an ARCANEE-provided 3D layer (“ARCANEE 3D Layer”). The cartridge API is intentionally **high-level**: cartridges do not control GPU pipeline states, descriptor sets, or command buffers. Instead, they manipulate **scene state** (entities, components, camera, lights, environment, materials), and the runtime renders the active scene into the CBUF.

This chapter defines:

* The data model (scenes, entities, components)
* Resource handles and lifetime rules
* glTF loading semantics and asset mapping
* Transform semantics and coordinate conventions
* Camera and lighting model
* PBR material model (v0.1)
* Rendering stages and integration with the overall pass topology
* Optional post-processing controls (v0.1 minimal)
* Limits, performance constraints, and failure modes

---

## 7.2 Coordinate Conventions (Normative)

### 7.2.1 World Space and Units

* World units are arbitrary but SHOULD be treated as meters by convention.
* The coordinate system MUST be consistent across platforms and documented. v0.1 mandates the following:

  * **Right-handed** world space
  * +X right, +Y up, +Z forward (or +Z toward viewer); the runtime MUST define one and apply it consistently.
  * The API MUST not expose ambiguous conventions; all matrices/quaternions must adhere to the chosen convention.

**Note:** If the chosen convention differs from glTF defaults, the runtime MUST apply deterministic conversion at import time so that loaded assets appear correct.

### 7.2.2 Transform Representation

Transforms are defined using:

* `pos = {x,y,z}` (float)
* `rot = {x,y,z,w}` (quaternion, float)
* `scale = {x,y,z}` (float)

The runtime MUST interpret quaternion components consistently and MUST document normalization rules:

* v0.1: runtime SHOULD normalize quaternions on assignment to avoid drift.

---

## 7.3 Data Model: Scenes, Entities, and Components (Normative)

### 7.3.1 Scenes

A **scene** is a container of entities and render settings. Exactly one scene may be **active** for rendering at a time.

API:

* `gfx3d.createScene() -> SceneHandle|0`
* `gfx3d.destroyScene(scene: SceneHandle) -> void`
* `gfx3d.setActiveScene(scene: SceneHandle) -> bool`
* `gfx3d.getActiveScene() -> SceneHandle`

Rules:

* Destroying an active scene MUST unset it (no active scene) or switch to a default empty scene (implementation-defined but deterministic).
* Scene handles are per-cartridge. A handle from another cartridge instance MUST be rejected.

### 7.3.2 Entities

Entities are lightweight identifiers used to attach components.

API:

* `gfx3d.createEntity(name: string = "") -> EntityId|0`
* `gfx3d.destroyEntity(e: EntityId) -> void`
* `gfx3d.setEntityName(e: EntityId, name: string) -> void`
* `gfx3d.getEntityName(e: EntityId) -> string|null`

Rules:

* Entity IDs are unique within a scene.
* Destroying an entity MUST destroy all its components deterministically.

### 7.3.3 Required Component: Transform

Every entity has an implicit transform component.

API:

* `gfx3d.setTransform(e: EntityId, pos: table, rot: table, scale: table) -> bool`
* `gfx3d.getTransform(e: EntityId) -> table|null`

  * returns `{ pos={x,y,z}, rot={x,y,z,w}, scale={x,y,z} }`

Rules:

* Missing fields MUST fail safely (return false + last error).
* Scale components MUST be finite and non-zero for correct rendering; if invalid, fail safely.

### 7.3.4 Mesh Renderer Component

A mesh renderer binds a mesh and a material to an entity.

API:

* `gfx3d.addMeshRenderer(e: EntityId, mesh: MeshHandle, material: MaterialHandle) -> bool`
* `gfx3d.removeMeshRenderer(e: EntityId) -> void`
* `gfx3d.setVisible(e: EntityId, visible: bool) -> void`

Rules:

* An entity may have at most one mesh renderer in v0.1.
* If a mesh or material handle is invalid, the call fails safely.

### 7.3.5 Light Component

Lights are created as handles and attached to entities.

API:

* `gfx3d.createLight(type: string) -> LightHandle|0`

  * `type ∈ { "directional", "point", "spot" }`
* `gfx3d.destroyLight(light: LightHandle) -> void`
* `gfx3d.attachLight(e: EntityId, light: LightHandle) -> bool`
* `gfx3d.detachLight(e: EntityId) -> void`
* `gfx3d.setLightParams(light: LightHandle, params: table) -> bool`

Light parameter tables (normative minimum):

**Directional**

* `{ color=ColorRGB, intensity=float, direction={x,y,z}? }`

  * If attached to entity, direction is derived from entity rotation; `direction` field MAY be ignored.

**Point**

* `{ color=ColorRGB, intensity=float, range=float }`

**Spot**

* `{ color=ColorRGB, intensity=float, range=float, innerAngle=float, outerAngle=float }`

Rules:

* `ColorRGB` is `{r,g,b}` in 0..1 floats (not u32), to avoid ambiguity in linear space.
* Angles are radians.
* Invalid values (negative intensity, outerAngle < innerAngle, non-finite values) MUST fail safely.

### 7.3.6 Camera Component

Cameras are created as handles and set as active.

API:

* `gfx3d.createCamera() -> CameraHandle|0`
* `gfx3d.destroyCamera(cam: CameraHandle) -> void`
* `gfx3d.setCameraPerspective(cam: CameraHandle, fovY: float, near: float, far: float) -> bool`
* `gfx3d.setCameraOrthographic(cam: CameraHandle, height: float, near: float, far: float) -> bool` (optional but recommended)
* `gfx3d.setCameraView(cam: CameraHandle, eye: table, at: table, up: table) -> bool`
* `gfx3d.attachCamera(e: EntityId, cam: CameraHandle) -> bool` (optional)
* `gfx3d.setActiveCamera(cam: CameraHandle) -> bool`
* `gfx3d.getActiveCamera() -> CameraHandle`

Rules:

* Exactly one active camera is used during `gfx3d.render()`. If none is active, `gfx3d.render()` MUST fail safely or render nothing deterministically (implementation-defined; Workbench should warn).
* `near` MUST be > 0 and < `far`.

---

## 7.4 Resource Handles and Lifetime (Normative)

### 7.4.1 Handle Types

* `SceneHandle`
* `EntityId`
* `MeshHandle`
* `MaterialHandle`
* `TextureHandle`
* `CameraHandle`
* `LightHandle`
* `AnimHandle` (animations; v0.1 optional)

### 7.4.2 Ownership and Cleanup

* All handles are owned by the cartridge instance.
* On cartridge stop or full reload (Chapter 4), the runtime MUST destroy all cartridge-owned 3D resources and invalidate handles.

### 7.4.3 Validation

Every `gfx3d.*` call MUST validate:

* handle existence
* handle type
* handle ownership (belongs to current cartridge)
* referenced scene is active/exists where relevant

On failure: return `false`/`0`/`null` and set `sys.getLastError()`.

---

## 7.5 Asset Loading: glTF (Normative)

### 7.5.1 glTF as Standard Format

ARCANEE v0.1 standard 3D interchange format is **glTF 2.0**:

* `.gltf` (JSON + external buffers)
* `.glb` (binary)

### 7.5.2 Loading API

* `gfx3d.loadGLTF(path: string, opts: table=null) -> table|null`

Return table (normative shape):

* `{ scene: SceneHandle, root: EntityId, meshes: array<MeshHandle>, materials: array<MaterialHandle>, textures: array<TextureHandle>, animations: array<AnimHandle> }`

Options (v0.1 recommended):

* `mergeMeshes: bool=false`
* `generateTangents: bool=true`
* `flipV: bool=false` (if needed by texture pipeline; SHOULD default to correct glTF interpretation)
* `scale: float=1.0` (uniform import scale)

### 7.5.3 VFS and Permission Rules

* `path` MUST be a VFS path (`cart:/`, `save:/`, `temp:/`), subject to sandbox rules (Chapter 3).
* External file references inside `.gltf` MUST be resolved relative to the `.gltf` file directory within `cart:/` (or the same namespace), and MUST NOT escape it.

### 7.5.4 Deterministic Loading

Loading results MUST be deterministic given identical source files and options:

* Entity creation order MUST be deterministic.
* Mesh/material indexing in returned arrays MUST be deterministic.
* Any procedural generation (e.g., tangent computation) MUST be deterministic.

---

## 7.6 Material Model (PBR) (Normative)

### 7.6.1 PBR Metallic-Roughness Base Model

v0.1 materials implement the glTF 2.0 metallic-roughness PBR model at minimum.

Material properties (normative minimum):

* Base color: `baseColorFactor` (RGBA) and/or `baseColorTexture`
* Metallic/Roughness: factors and/or combined texture
* Normal map texture (+ scale)
* Emissive factor and/or emissive texture
* Occlusion texture (optional but recommended)

### 7.6.2 Material API

Creation:

* `gfx3d.createMaterial() -> MaterialHandle|0`
* `gfx3d.destroyMaterial(m: MaterialHandle) -> void`

Setters (normative minimum):

* `gfx3d.matSetBaseColor(m, color: table) -> bool`

  * `color={r,g,b,a}` floats 0..1
* `gfx3d.matSetBaseColorTex(m, tex: TextureHandle|null) -> bool`
* `gfx3d.matSetMetallicRoughness(m, metallic: float, roughness: float) -> bool`
* `gfx3d.matSetMetallicRoughnessTex(m, tex: TextureHandle|null) -> bool`
* `gfx3d.matSetNormalTex(m, tex: TextureHandle|null, scale: float=1.0) -> bool`
* `gfx3d.matSetEmissive(m, color: table) -> bool`

  * `color={r,g,b}` floats
* `gfx3d.matSetEmissiveTex(m, tex: TextureHandle|null) -> bool`
* `gfx3d.matSetAlphaMode(m, mode: string, cutoff: float=0.5) -> bool`

  * `mode ∈ { "opaque", "mask", "blend" }`
* `gfx3d.matSetDoubleSided(m, on: bool) -> bool`

### 7.6.3 Texture API (v0.1 minimal)

* `gfx3d.loadTexture(path: string, opts: table=null) -> TextureHandle|0`
* `gfx3d.destroyTexture(t: TextureHandle) -> void`
* `gfx3d.getTextureSize(t: TextureHandle) -> (int w, int h)`

Texture options (recommended):

* `srgb: bool` (default true for baseColor/emissive, false for normal/metallicRoughness/occlusion)
* `mips: bool` (default true)
* `filter: "linear"|"nearest"` (default linear; Workbench may override)

---

## 7.7 Rendering Settings: Environment and Post Effects (v0.1)

### 7.7.1 Environment Lighting (IBL)

ARCANEE v0.1 SHOULD support environment lighting via HDR environment maps.

API:

* `gfx3d.setEnvironmentHDR(path: string, opts: table=null) -> bool`
* `gfx3d.clearEnvironment() -> void`

Options (recommended):

* `intensity: float=1.0`
* `rotationY: float=0.0` (radians)

### 7.7.2 Tonemapping

* `gfx3d.setTonemapper(mode: string) -> bool`

  * `mode ∈ { "aces", "reinhard", "none" }`

### 7.7.3 Post Effects (Minimal Controls)

v0.1 MAY expose basic toggles; unsupported features must fail safely:

* `gfx3d.enableTAA(on: bool) -> bool`
* `gfx3d.setBloom(params: table) -> bool`
* `gfx3d.setSSAO(params: table) -> bool`

If these are not implemented in v0.1, the functions MUST exist and return `false` with a clear error (for forward compatibility).

---

## 7.8 Render Execution (`gfx3d.render`) (Normative)

### 7.8.1 Call Semantics

* `gfx3d.render() -> bool`

`gfx3d.render()` renders the active scene into the **currently bound target**:

* Default target is CBUF.
* If a `gfx` surface is currently bound, the 3D renderer MUST either:

  * render to CBUF only and ignore surface binding (v0.1 allowed), OR
  * support rendering to that surface (advanced).
    In v0.1, the recommended policy is: **3D always renders to CBUF** and surfaces are 2D-only, unless explicitly extended.

The runtime MUST document which policy is implemented. For simplicity and correctness, v0.1 RECOMMENDS **3D → CBUF only**.

### 7.8.2 Ordering Relative to 2D

The runtime MUST render 3D prior to compositing the 2D canvas (Chapter 5). Cartridges do not control pass ordering.

### 7.8.3 Deterministic Visibility

Given identical transforms and camera, the set of visible draw calls MUST be deterministic. GPU-dependent floating-point differences may affect edge cases at frustum boundaries; v0.1 does not require bit-identical culling across GPUs, but requires stable (non-random) behavior.

---

## 7.9 Limits and Performance Constraints (Normative)

### 7.9.1 Scene Limits

ARCANEE MUST enforce limits to prevent pathological workloads (policy-defined defaults; values may be surfaced in Workbench):

* Maximum entities per scene
* Maximum mesh renderers
* Maximum lights affecting a single object (or clustered limit)
* Maximum texture memory per cartridge (approximate VRAM budget)
* Maximum triangles per frame (soft limit; warn in Workbench)

If limits are exceeded:

* Calls MUST fail safely and set last error, or
* The runtime MAY clamp/ignore excess deterministically (must be documented).

### 7.9.2 Asynchronous Loading

The runtime MAY load textures/meshes asynchronously using worker threads, but MUST ensure:

* `gfx3d.loadGLTF` returns deterministically
* Partially loaded assets have deterministic placeholder behavior (e.g., a default material) until fully ready
* Cartridges can query readiness deterministically if needed (v0.1 optional)

Recommended (v0.1): `loadGLTF` is synchronous in Workbench and may be asynchronous in Player mode only if readiness is exposed.

### 7.9.3 Shader/Pipeline Compilation

Pipeline creation may be expensive; the runtime SHOULD cache pipeline states and compiled shaders. Caching MUST be stable and must not leak across cartridges in ways that violate sandbox expectations.

---

## 7.10 Error Handling (Normative)

All `gfx3d.*` APIs MUST:

* Validate arguments and handles
* Enforce VFS sandbox rules for asset paths
* Fail safely with `false/0/null` and set `sys.getLastError()`

In Workbench (Dev Mode), failures SHOULD also:

* Emit a diagnostic log entry containing the API call name and normalized path/handle information (without leaking host filesystem paths in Player mode).

---

## 7.11 Compliance Checklist (Chapter 7)

An implementation is compliant with Chapter 7 if it:

1. Implements the `gfx3d.*` scene/entity/camera/light APIs defined in §§7.3–7.4.
2. Implements deterministic glTF loading via VFS with canonical path resolution and deterministic ordering.
3. Provides a PBR material model consistent with metallic-roughness semantics and exposes material setters.
4. Renders the active scene into CBUF prior to 2D canvas composition and present.
5. Enforces handle validation, sandbox constraints, and safe failure behavior.
6. Enforces scene and resource limits to prevent runaway workloads and provides diagnostics in Workbench.
---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
