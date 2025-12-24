# Decision Log — ARCANEE

This log is append-only. IDs are stable.

## [DEC-01] Primary target user experience
- Status: **Accepted**
- Decision: ARCANEE is a fantasy console runtime that runs cartridges with deterministic stepping and an integrated Workbench.
- Rationale: aligns specs + README + v1.0 goal.
- Consequences: Workbench becomes first-class subsystem (Ch8).

## [DEC-02] Cross-platform intent
- Status: **Accepted**
- Decision: Support Linux/Windows/macOS for v1.0.

## [DEC-03] Rendering strategy intent
- Status: **Accepted**
- Decision: Multi-backend rendering via Diligent rather than raw Vulkan/Metal/DX12 in v1.0.

## [DEC-04] Public API boundary (C++ SDK)
- Status: **Accepted**
- Decision: v1.0 does not promise a stable C++ SDK ABI; “public API” is script/file formats + CLI.
- Consequences: `/include/` optional until SDK is required.

## [DEC-05] Dependency + supply-chain policy
- Status: **Accepted**
- Decision: FetchContent is allowed for v1.0, but all deps must be pinned and recorded; CI must be reproducible.

## [DEC-06] Sandbox/security posture
- Status: **Accepted**
- Decision: Treat cartridges as untrusted; enforce strict filesystem sandbox; validate bindings; impose limits.

## [DEC-07] Packaging/distribution
- Status: **Accepted**
- Decision: v1.0 ships as single-folder portable build.

## [DEC-08] v1.0 deliverable composition
- Status: **Accepted**
- Decision: runtime + workbench.

## [DEC-09] Target OS set
- Status: **Accepted**
- Decision: Linux + Windows + macOS.

## [DEC-10] GPU backend policy
- Status: **Accepted**
- Decision: Diligent is the primary renderer abstraction.

## [DEC-11] Graphics feature subset
- Status: **Accepted**
- Decision: 2D + 3D included in v1.0.

## [DEC-12] Determinism contract
- Status: **Accepted**
- Decision: determinism covers simulation + input only.

## [DEC-13] Threat model
- Status: **Accepted**
- Decision: untrusted downloadable cartridges; strict sandbox; never read/write outside cartridge scope.

## [DEC-14] Packaging target
- Status: **Accepted**
- Decision: portable single-folder build.

## [DEC-15] Dependency management preference
- Status: **Accepted**
- Decision: keep CMake FetchContent.

## [DEC-16] Scripting surface
- Status: **Accepted**
- Decision: Squirrel is the long-term scripting choice.

## [DEC-17] Networking
- Status: **Accepted**
- Decision: out-of-scope for v1.0.

## [DEC-18] Blueprint vs specs precedence
- Status: **Accepted**
- Decision: `/blueprint/` is normative; `/specs/` is reference.

## [DEC-19] `/include/` requirement
- Status: **Accepted**
- Decision: optional for v1.0 (no stable C++ SDK ABI in v1.0).

## [DEC-20] Diligent backend selection rule
- Status: **Accepted**
- Decision: Windows DX12 default; macOS Metal; Linux Vulkan; optional OpenGL fallback.

## [DEC-21] Error model
- Status: **Accepted**
- Decision: use `Status/StatusOr` style error returns (no exceptions as primary control flow).

## [DEC-22] Public API definition
- Status: **Accepted**
- Decision: v1.0 public API is script bindings + file formats + CLI.

## [DEC-23] Allocator strategy
- Status: **Accepted**
- Decision: FrameArena + ResourceHeap (tracked), to stabilize frame times.

## [DEC-24] GPU sync complexity
- Status: **Accepted**
- Decision: avoid multi-queue/multi-threaded rendering in v1.0.

## [DEC-25] Workbench UI stack
- Status: **Accepted**
- Decision: ImGui + docking for v1.0 workbench.

## [DEC-26] Hot reload strategy
- Status: **Accepted**
- Decision: scripts hot reload; assets best-effort; polling fallback watcher in v1.0.


