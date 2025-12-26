<!-- dependency_policy.md -->

# Dependency Policy (Normative)

This policy governs selection, approval, integration, licensing, and updating of third-party dependencies for repositories governed by the DET CDD blueprint system.

It is **normative** unless a higher-precedence rule overrides via DEC + enforcement.

---

## 1. Goals

- Ensure supply-chain security and license compliance.
- Maintain deterministic builds (pinned versions, reproducible fetch).
- Minimize dependency sprawl.
- Provide clear rules for vendoring vs package manager usage.
- Enforce dependency rules through CI gates.

---

## 2. Definitions

- **Dependency**: any third-party code/library/tool used at build time or runtime.
- **Direct dependency**: referenced by project build or source directly.
- **Transitive dependency**: required by a direct dependency.
- **Vendoring**: copying dependency source into the repo (e.g., under `/third_party`).
- **Pinned version**: explicit immutable identifier (tag + commit hash; or exact archive hash).
- **SBOM**: software bill of materials (e.g., SPDX format).
- **SLSA**: supply-chain levels for software artifacts (conceptual; see §10 guidance).

---

## 3. Approval and documentation (MUST)

### 3.1 Dependency record required

Every direct dependency MUST be recorded in a dependency manifest. Recommended location:
- `/docs/dependencies.md` (non-blueprint)
or
- `/_blueprint/project/ch3_components.md` dependency section plus a dedicated non-blueprint manifest.

The record MUST include:
- name
- purpose
- license
- version pin (tag + commit, or hash)
- source URL
- whether vendored or fetched
- update policy (cadence and owner)

### 3.2 Decision requirement for non-trivial deps

Adding a non-trivial dependency (runtime library, networking stack, crypto, serialization, GPU SDK wrapper) MUST be accompanied by a DEC including:
- alternatives considered
- security and maintenance implications
- performance impact
- how it is pinned and updated
- enforcement gates

“Non-trivial” is any dependency that:
- affects runtime behavior, or
- ships to users, or
- introduces significant transitive closure, or
- involves native code/toolchains.

---

## 4. Default strategy (widely recognized, rational)

### 4.1 Prefer minimal dependencies

- Prefer standard library first.
- Prefer small, well-maintained libraries with clear licensing.
- Avoid “framework” dependencies unless they strongly reduce complexity.

### 4.2 Prefer pinned and reproducible fetch

Default recommended approaches (choose via DEC):
- **FetchContent** with pinned `GIT_TAG` set to a commit hash (not a moving tag)
- **CPM.cmake** pinned to commit + dependency pins
- **vcpkg** manifest mode with locked baseline and versions
- **Conan** with lockfiles

Rules:
- Builds MUST NOT fetch moving targets (e.g., `main`, `master`, “latest”).
- Builds MUST NOT depend on network access in “offline” CI mode unless explicitly approved via DEC.

### 4.3 Vendoring policy (default)

Vendoring is allowed when:
- dependency is small,
- updates are infrequent,
- supply-chain risk is reduced by making source visible,
- license permits redistribution.

Vendored dependencies MUST:
- include upstream license files
- include an `UPSTREAM.md` file (recommended) recording:
  - upstream URL
  - version/commit
  - local patches
- be pinned to upstream commit hash

Large dependencies SHOULD NOT be vendored unless DEC + justification exists.

---

## 5. License policy (MUST)

### 5.1 Allowed licenses (default allowlist)

Default allowlist (widely recognized as permissive):
- MIT
- BSD-2-Clause, BSD-3-Clause
- Apache-2.0

Allowed with caution (DEC recommended depending on distribution model):
- MPL-2.0 (file-level copyleft)
- LGPL (dynamic linking constraints; generally avoid in static distribution)

Generally forbidden for proprietary distribution unless legal approval exists (DEC + legal gate):
- GPL-2.0/3.0 (strong copyleft)

### 5.2 License documentation

- Each dependency MUST have its license recorded.
- CI SHOULD run a license scan and fail on forbidden licenses.

---

## 6. Security policy (MUST)

### 6.1 Vulnerability management

- CI MUST include a dependency vulnerability scan step when feasible.
- For C++ projects, acceptable approaches include:
  - GitHub Dependabot alerts (if on GitHub)
  - OSV scanner for source dependencies (where supported)
  - SCA tools used by org (document via DEC)

If scanning cannot be done, add DEC + failing-fast gate requiring periodic manual review.

### 6.2 Integrity and provenance

- Dependencies MUST be pinned by immutable identifiers.
- For archive downloads:
  - MUST verify SHA256 (or stronger) hash.
- For git fetches:
  - MUST pin commit hash.
- Prefer signed tags/releases when available; document verification.

### 6.3 No hidden fetches

Build scripts MUST NOT:
- curl/wget arbitrary URLs without verification,
- execute remote scripts.

Any exception requires DEC + security gate.

---

## 7. Toolchain dependencies (build-time)

Build-time tools (clang-format, clang-tidy, python scripts, code generators) MUST be:
- version-pinned (via CI images, toolchain files, or explicit download with hash)
- documented in Ch7 toolchain matrix.

If python tooling is used:
- pin requirements in `requirements.txt` or lockfile (recommended).

---

## 8. Updating dependencies (normative)

### 8.1 Update cadence

Default:
- security updates: urgent (within days to weeks)
- routine updates: quarterly or as needed

Projects MUST document:
- who owns updates
- how updates are tested (CI full matrix)
- how compatibility is assessed (SemVer + API/ABI rules)

### 8.2 Update process

For each dependency update:
- create a CR
- update pins and manifests
- run full CI matrix
- run benchmarks if hotpath/perf budgets are relevant

---

## 9. Enforcements (MUST)

CI MUST include gates that fail on:
- unpinned dependencies (moving tags/branches)
- network fetches without hash/commit pins
- missing license records for dependencies
- forbidden licenses (unless DEC + explicit allow with legal gate)
- dependency updates without accompanying CR (recommended gate)

Validator SHOULD check:
- dependency manifest exists and entries are complete (if standardized location is known)
- blueprint references to dependency policy exist

---

## 10. Recommended advanced practices (non-normative)

- Generate an SBOM (SPDX) as a CI artifact.
- Use SLSA provenance where supported by CI system.
- Use reproducible builds flags (e.g., `-Wdate-time` avoidance, `SOURCE_DATE_EPOCH`).

---

## 11. Widely recognized “safe defaults” catalog (informative)

Common C++ dependencies and typical rationale (approval still requires DEC when non-trivial):
- `fmt` (formatting) — small, widely used, permissive license
- `spdlog` (logging) — widely used, but evaluate against determinism budgets
- `gtest` (testing) — standard
- `benchmark` (perf microbench) — standard

Use only if they satisfy:
- license allowlist
- pinned versions
- performance and determinism constraints

---

## 12. Policy changes

Changes to this policy MUST be introduced via CR and MUST include:
- enforcement/gate changes,
- migration guidance for existing projects.
