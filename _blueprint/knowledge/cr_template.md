<!-- _blueprint/knowledge/cr_template.md -->

# Change Request Template — CR-XXXX (Normative)
This template defines the canonical format for **Change Requests** used to evolve a project from v1.0 to v2.0 and beyond while preserving traceability, compatibility, and quality gates.

> **Rule:** Any significant change MUST be represented as a CR (`/blueprint/cr/CR-XXXX.md`) and MUST update `/blueprint` (Blueprint + checklist) before or alongside code changes.

---

## Header
- **CR ID:** `[CR-XXXX]`
- **Title:** <short, descriptive>
- **Status:** Draft | Approved | Implemented | Rejected
- **Owner:** <name/handle>
- **Created:** YYYY-MM-DD
- **Target Release:** v<MAJOR>.<MINOR>.<PATCH>
- **Related Issues/Links:** <URLs/refs>
- **Scope Type:** Feature | Breaking Change | Refactor | Performance | Security | Dependency | Tooling | Docs

---

## 1) Summary
**One paragraph** describing *what changes* and *why*, in user/product terms.

---

## 2) Motivation & Goals
### 2.1 Motivation
- What problem/opportunity does this address?
- What user-facing or operational pain exists today?

### 2.2 Goals (MUST be measurable)
- Goal A: <measurable outcome>
- Goal B: <measurable outcome>

### 2.3 Non-Goals
- Explicitly list what is NOT being attempted.

---

## 3) Impacted Blueprint IDs (Traceability)
List all IDs impacted/introduced by this change.

| Category | IDs |
|---|---|
| Requirements | `[REQ-..]` |
| Architecture | `[ARCH-..]` |
| Decisions | `[DEC-..]` |
| Memory | `[MEM-..]` |
| Concurrency | `[CONC-..]` |
| API/ABI | `[API-..]` |
| Python | `[PY-..]` |
| Build | `[BUILD-..]` |
| Tests | `[TEST-..]` |
| Versioning/Lifecycle | `[VER-..]` |

**Rule:** If new major decisions are required, add new **[DEC-XX]** entries in `decision_log.md` and reference them here.

---

## 4) Compatibility & Versioning
### 4.1 API Compatibility
- **API breaking?** Yes/No  
- If yes: list removed/changed symbols, behavior changes, and new alternatives.

### 4.2 ABI Compatibility (if applicable)
- **ABI breaking?** Yes/No  
- If yes: explain what breaks (layout, vtables, symbol changes, calling conventions) and mitigation.

### 4.3 SemVer Classification (MUST)
- **This change is:** MAJOR | MINOR | PATCH  
- Justification (tie to `[VER-01]` rules).

### 4.4 Deprecations
- Deprecations introduced (symbols/APIs), include:
  - `since: vX.Y`
  - `removal_target: vA.B` (or “next major”)
  - replacement guidance.

### 4.5 Migration Plan (Required if MAJOR, recommended otherwise)
- Does this require `MIGRATION.md` updates? Yes/No
- Provide “Before → After” code examples (brief).
- Tooling: clang-tidy checks, scripts, or compile-time warnings (if available).

---

## 5) Technical Design (Concise but Concrete)
### 5.1 Proposed Design
- Key design points (bullets).
- New/changed modules and boundaries.
- Public API sketches (signatures only) if API changes.

### 5.2 Data / Memory Implications
- Ownership changes, allocator changes, layout changes.
- Performance-critical hot paths affected.

### 5.3 Concurrency Implications
- Thread-safety changes, synchronization changes.
- Potential races/deadlocks and mitigations.

### 5.4 Alternatives Considered
- Option 1: <summary + why rejected>
- Option 2: <summary + why rejected>

> **Rule:** If the design choice is significant, record a corresponding **[DEC-XX]** in `decision_log.md`.

---

## 6) Performance Impact
### 6.1 Budgets Affected
Reference existing budgets and specify any changes:
- Latency: p50/p99/p999 impact
- Throughput impact
- Memory steady/peak impact
- Startup impact
- GPU budget impact (if applicable)

### 6.2 Benchmarks
- New benchmarks to add (names + what they measure).
- Regression thresholds (what should fail CI).

---

## 7) Security & Reliability Impact
- New attack surface? input validation? sandbox constraints?
- Crash-only vs error handling implications.
- Changes to dependencies (CVE considerations, licenses).

If relevant:
- SBOM changes (add/remove deps).
- Hardening flags or sanitizer coverage updates.

---

## 8) Test Plan (MUST be explicit)
### 8.1 Tests to Add/Update
- Unit tests: <list>
- Integration tests: <list>
- Concurrency tests (stress/soak): <list>
- Fuzz tests (if applicable): <list>
- Python tests (if applicable): <list>

### 8.2 Verification Commands (Runnable)
Provide exact commands (examples; adjust to project presets):
- `cmake --preset dev && cmake --build --preset dev`
- `ctest --preset dev`
- `ctest --preset asan` / `ctest --preset tsan` / `ctest --preset ubsan`
- Bench: `<bench command>`

### 8.3 Acceptance Criteria (MUST be measurable)
- AC-1: <measurable>
- AC-2: <measurable>
- AC-3: <measurable>

---

## 9) Implementation Plan (Checklist-friendly)
Break work into steps that can map into `implementation_checklist.yaml`.

| Step | Description | Refs (IDs) | Artifacts | DoD |
|---:|---|---|---|---|
| 1 | <setup> | `[BUILD-..]` | files/targets | passes build |
| 2 | <impl> | `[API-..] [MEM-..]` | code | tests pass |
| 3 | <verify> | `[TEST-..]` | tests/bench | meets thresholds |
| 4 | <docs/migrate> | `[VER-..]` | MIGRATION/CHANGELOG | updated |

---

## 10) Rollout & Risk Mitigation
- Rollout strategy (feature flag? gradual enable?).
- Backout plan (how to revert safely).
- Risks (technical/product) and mitigations.

---

## 11) Required Updates (Gate Checklist)
Mark each item as ✅ when done.

- [ ] Update `/blueprint/blueprint_vX.Y.md` (and/or add new snapshot)
- [ ] Update `/blueprint/decision_log.md` with any new [DEC-XX]
- [ ] Update `/blueprint/implementation_checklist.yaml` (new tasks/refs)
- [ ] Update `CHANGELOG.md`
- [ ] Update `MIGRATION.md` (required for MAJOR)
- [ ] Add/update tests per §8
- [ ] Ensure **no** `[TEMP-DBG]` markers remain
- [ ] All CI quality gates pass (format/lint/sanitizers/tests/bench thresholds)

---

## Notes
- Keep CRs **small** when possible; large changes should be split into multiple CRs.
- If the CR is rejected, record the reason (e.g., performance risk, scope creep) to avoid rework later.
