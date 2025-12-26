<!-- cr_template.md -->

# Change Request (CR) Template (Normative)

A Change Request (CR) is the **only** approved mechanism for evolving the blueprint SoT (`/_blueprint/`) over time.
CRs:
- justify changes,
- define explicit deltas,
- enumerate enforcement updates (tests/gates),
- update versioning expectations (SemVer),
- provide migration guidance.

This template is **normative** and must be used for files named `/_blueprint/vM.m/CR-XXXX.md`.

---

## 1. CR file naming and placement

### 1.1 Naming

CR files MUST be named:
- `CR-0001.md`, `CR-0002.md`, ... (4 digits, zero padded)

### 1.2 Placement

CR files MUST live in a version overlay directory:
- `/_blueprint/vM.m/CR-XXXX.md`

### 1.3 References

Each CR MUST be referenced from:
- the overlay’s `release.md` (CR list), and
- any updated chapter or decision delta as applicable.

---

## 2. CR schema (MUST)

Each CR MUST contain the following sections, in this order.

### 2.1 Header block (MUST)

```md
# CR-0000 — <Short descriptive title>
**Status:** draft|accepted|implemented  
**Owner:** <team or role>  
**Target Version:** M.m.p (must match overlay release.md “Version”)  
**Date:** YYYY-MM-DD  
**Related:** DEC-xxxxx, REQ-xxxxx, ARCH-xxxxx, API-xxxxx, BUILD-xxxxx, TST-xxxxx (as applicable)
```

Rules:
- Status MUST start as `draft` and become `accepted` once approved.
- `Target Version` MUST match the version declared in `release.md`.

### 2.2 Motivation (MUST)

Explain:
- what problem is solved,
- why now,
- risks of not doing it,
- constraints that shaped the proposal.

### 2.3 Summary of Changes (MUST)

A concise bullet list describing what will change.

Example:

- Add deterministic time source abstraction (`API-00110`)
- Introduce new CI gate preventing `[TEMP-DBG]` leftovers (`BUILD-00201`)
- Update Ch6 concurrency model to include backpressure strategy

### 2.4 Detailed Deltas (MUST)

Provide a structured change list, grouped by artifact:

- **Blueprint chapters**
  - `/_blueprint/project/chX_*.md` (baseline) or overlay `chX_*.md`
  - describe edits by section/heading
- **Rules / knowledge**
  - list files added/modified
- **Build / Tooling**
  - CMake targets/presets changes
  - new scripts (composer/validator, linters)
- **Code changes** (if applicable)
  - list directories/files and a short rationale

Each delta SHOULD reference relevant IDs (REQ/DEC/API/TST/BUILD).

### 2.5 Compatibility Impact (SemVer) (MUST)

State explicitly:

- **Impact type**: `PATCH | MINOR | MAJOR`
- **Why** the chosen bump is correct, referencing SemVer rules in Ch9.
- **Compatibility notes**:
  - source compatibility
  - ABI compatibility (if a library)
  - behavior changes
  - performance impact

If impact is uncertain, record a DEC and add a gate requiring confirmation/measurement.

### 2.6 Migration Plan (MUST when needed)

If any user-facing behavior or API changes:
- list step-by-step migration instructions
- include code examples if necessary (keep short; prefer references to docs/tests)
- identify deprecations and timelines
- include rollback strategy if feasible

### 2.7 Enforcement Plan (MUST)

This is the most important section.

List:
- **New or updated tests**
  - IDs and descriptions
  - exact commands
- **New or updated CI gates**
  - IDs and descriptions
  - failure messages and remediation hints
- **Validator updates**
  - checks added (e.g., schema validation, ID scan, inheritance resolution)
- **Checklist updates**
  - new steps in `implementation_checklist.yaml` or `checklist_delta.yaml`

Every CR MUST introduce or update at least one of:
- tests, or
- gates, or
- validator checks.

### 2.8 Rollout Plan (MUST)

Describe:
- how changes will be implemented in phases (if relevant),
- who owns what,
- timeline expressed as sequence of milestones (avoid dates unless necessary),
- what “done” means (acceptance criteria).

### 2.9 Risks and Mitigations (MUST)

Provide:
- risk list (technical, schedule, correctness, performance, portability)
- mitigation per risk (tests, gates, staged rollout)

### 2.10 Alternatives Considered (MUST)

List alternatives and why they were not chosen.
If no alternatives, record a DEC explaining why.

### 2.11 Open Questions (MUST if any)

If any unknowns remain, list them and define failing-fast gates to prevent shipping without answers.

### 2.12 Appendix (Optional)

- Links
- Design diagrams (ASCII or references)
- Supporting benchmarks
- Dependency/license notes

---

## 3. CR quality bar (normative checks)

CI/validator SHOULD fail if:
- Required sections are missing.
- `Target Version` does not match `release.md`.
- Compatibility impact is not specified.
- Enforcement plan is missing tests/gates/validator changes.
- CR is referenced from release.md but file is missing.
- CR introduces new requirements without adding tests and checklist steps.

---

## 4. Minimal example (copy/paste)

```md
# CR-0007 — Add CI gate for TEMP debug markers
**Status:** draft  
**Owner:** Build/CI  
**Target Version:** 1.4.2  
**Date:** 2025-02-01  
**Related:** TEMP-DBG-00001, BUILD-00210

## Motivation
TEMP debug code must never ship; we need automated detection.

## Summary of Changes
- Add validator check scanning for “[TEMP-DBG]”
- Add CI job “gate-temp-dbg” that fails on any hit

## Detailed Deltas
- /_blueprint/rules/temp_dbg_policy.md: define marker rule
- /tools/blueprint/validate.py: implement scan
- /.github/workflows/ci.yml: add job

## Compatibility Impact (SemVer)
Impact type: PATCH  
No API change; only strengthens CI.

## Migration Plan
None.

## Enforcement Plan
- CI Gate: BUILD-00210 — gate-temp-dbg (fails if any “[TEMP-DBG]”)
- Validator: add scan step; error includes filename+line
- Checklist: add step “Run blueprint validator locally” referencing BUILD-00210

## Rollout Plan
1) Implement validator scan
2) Add CI job
3) Enable branch protection requiring the job

## Risks and Mitigations
- Risk: false positives in docs → Mitigation: restrict scan to src/include/tests by rule

## Alternatives Considered
- Pre-commit only → rejected; can be bypassed.
```

---

## 5. Notes (non-normative)

- Keep CRs small and composable: one intent per CR.
- If a CR changes multiple areas, split into separate CRs unless they must land together.
- Always tie new rules to enforcement, or the blueprint becomes aspirational rather than deterministic.
