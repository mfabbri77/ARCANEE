# Dependency Policy (Normative)
This document defines a robust policy for selecting, pinning, upgrading, and governing **third-party dependencies** in native C++ repositories (plus optional Python bindings).  
It is designed to reduce technical debt, avoid supply-chain risks, and keep builds reproducible across versions.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Goals
- Keep builds reproducible and deterministic.
- Minimize dependency sprawl and ABI headaches.
- Ensure license/security compliance.
- Make upgrades deliberate and test-backed.
- Support long-term lifecycle (v1.0 → v2.0) with predictable change control.

---

## 2) Selection Rules (Mandatory)
Before adding a dependency, answer:
- What capability do we need that we cannot implement cheaply and safely?
- Is the dependency widely used and maintained?
- Does it support our platform matrix and toolchains?
- Is the license acceptable for the distribution model?

**Rule:** Prefer fewer, stronger dependencies over many small ones.

---

## 3) Allowed/Discouraged Patterns
### 3.1 Preferred
- Single dependency manager: **vcpkg (manifest mode)** OR **conan** (choose one).
- Header-only dependencies only when they are stable and do not bloat compile times excessively.
- System libraries only when they are stable and available across targets (e.g., OS APIs).

### 3.2 Discouraged
- Git submodules pointing to floating branches (non-deterministic).
- “FetchContent from main branch” in CMake.
- Vendor huge dependency trees without a strong reason.

### 3.3 Vendoring (Exception-only)
Vendoring is allowed only when:
- dependency is small and stable, OR
- licensing/compliance requires it, OR
- build environments forbid external fetching.

**Rule:** If vendored, pin exact version and keep it under `/third_party/` with license files.

---

## 4) Pinning & Reproducibility (Mandatory)
### 4.1 vcpkg
- Use manifest mode: `vcpkg.json`
- Pin baseline via `vcpkg-configuration.json` (or known-good commit/baseline)
- Use explicit overrides for critical libs (avoid surprise upgrades)

### 4.2 conan
- Use lockfiles and pinned versions
- Prefer profiles per platform/toolchain
- Ensure CI uses the same lockfiles

**Rule:** Dependencies must be pinned such that a build today equals a build tomorrow.

---

## 5) Public vs Private Dependencies (Mandatory)
- Public dependencies affect consumers of your library (transitive requirements).
- Private dependencies are implementation details.

Rules:
- Keep public deps minimal.
- Do not leak private deps in exported CMake usage requirements.
- Avoid exposing STL ABI-sensitive types across boundaries (see `cpp_api_abi_rules.md`).

---

## 6) Upgrade Policy (Mandatory)
### 6.1 Cadence
- Define a cadence (e.g., monthly/quarterly) or “as-needed” with security exceptions.
- Security fixes may be expedited.

### 6.2 Change control
- Any dependency addition/removal/major upgrade requires a CR (`[CR-XXXX]`).
- The CR must include:
  - why the change is needed
  - impact analysis (ABI/API/performance)
  - test plan
  - rollback plan

### 6.3 Staged upgrades
For risky upgrades:
- isolate in a branch/CR
- run full CI + sanitizers
- include a compatibility note in CHANGELOG

---

## 7) Security Policy (Recommended baseline)
- Track upstream advisories for critical deps.
- On CVE:
  - assess impact and exposure
  - patch or upgrade promptly
  - record the action in a CR and CHANGELOG if user-impacting

Optional but recommended for commercial distribution:
- SBOM generation (CycloneDX/SPDX) on release builds.

---

## 8) License Compliance (Mandatory)
- Every dependency must have a license compatible with the project’s distribution.
- Maintain a `THIRD_PARTY_NOTICES` or similar summary if shipping to customers.
- Avoid GPL/LGPL unless the distribution model explicitly supports it.

**Rule:** If license is uncertain, do not add the dependency until clarified.

---

## 9) Performance & Footprint Considerations
- Consider compile-time cost (templates, header-only heavy libs).
- Consider runtime overhead (allocations, logging, dynamic dispatch).
- Consider binary size, especially for shipped SDKs and Python wheels.

**Rule:** If a dependency is on the hot path, require benchmarks and regression thresholds.

---

## 10) Common Approved Libraries (Examples, non-binding)
Depending on project constraints, commonly acceptable choices:
- Logging: spdlog / glog
- CLI: cxxopts / CLI11
- JSON: nlohmann/json (careful with compile time) or rapidjson
- Tests: Catch2 / GoogleTest
- Bench: Google Benchmark
- Vulkan: volk + VMA (if allowed)

**Rule:** The Blueprint must explicitly list chosen deps and why.

---

## 11) Documentation Requirements (Mandatory)
Blueprint must include a dependency table with:
- name, version, license, purpose, public/private, upgrade notes

Repo should include:
- `vcpkg.json` or `conanfile.*`
- baseline/lockfiles
- documentation on how to bootstrap deps locally

---

## 12) Quick Checklist
- [ ] Single dependency manager chosen and documented
- [ ] All deps pinned via baseline/lockfiles
- [ ] Public deps minimized; private deps not leaked
- [ ] License checked for every dependency
- [ ] Security/CVE response policy defined
- [ ] Major upgrades done via CR with tests and rollback plan
