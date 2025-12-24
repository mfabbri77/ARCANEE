# Decision Log Template — decision_log.md (Normative)
This document defines the canonical, **append-only** format for recording architectural decisions.  
It preserves long-term traceability across versions (v1.0 → v2.0 → …) and prevents “lost rationale”.

> **Rule:** `decision_log.md` is **append-only**. Never rewrite history; add new entries or addendums.

---

## 1) Purpose
- Capture **why** decisions were made, not just what was built.
- Provide a stable reference for AI coding agents and humans.
- Support lifecycle governance: deprecations, migrations, compatibility, performance regressions.

---

## 2) Rules (Mandatory)
### 2.1 Append-only
- Existing entries must not be edited except for:
  - fixing typos that do not change meaning
  - adding an explicit **Addendum** section at the end of the same entry (preferred)
- If a decision changes, create a **new** `[DEC-XX]` entry that supersedes the old one and links to it.

### 2.2 One decision per ID
- Each `[DEC-XX]` should cover one major decision.
- Do not overload a single DEC with multiple unrelated choices.

### 2.3 Traceability linking
Each decision must link to:
- impacted requirements `[REQ-XX]`
- relevant architecture rules `[ARCH-XX]`
- affected API/ABI rules `[API-XX]` (if any)
- memory/concurrency/build/test/versioning rules as applicable

### 2.4 Alternatives are required
- Always include at least one alternative and a short rejection rationale.

### 2.5 Consequences must be explicit
- Include both pros and cons.
- Include risks and mitigations (or follow-up CRs).

---

## 3) Index (Optional but recommended)
Maintain a simple list for fast navigation:

- [DEC-01] <title> (v1.0) — status: Active
- [DEC-02] <title> (v1.0) — status: Active
- [DEC-03] <title> (v1.1) — status: Superseded by [DEC-07]
- …

---

## 4) Decision Entry Template
Copy/paste for each new entry:

---

### [DEC-XX] <Short Decision Title>
- **Date:** YYYY-MM-DD
- **Status:** Active | Superseded | Deprecated
- **Introduced In:** v<MAJOR>.<MINOR>.<PATCH>
- **Supersedes:** (optional) [DEC-YY]
- **Superseded By:** (optional) [DEC-ZZ]
- **Related CRs:** (optional) [CR-XXXX], [CR-YYYY]
- **Scope:** (module/component/subsystem)

#### Context
What is the problem and what constraints drive this decision?
- Constraints: [REQ-..], [ARCH-..], [VER-..] (list)
- Assumptions: [META-..] (list)

#### Decision
State the decision in one clear sentence, then provide details.
- Decision statement:
- Detailed notes:
  - Interfaces affected: [API-..] (if any)
  - Memory/ownership impact: [MEM-..]
  - Concurrency impact: [CONC-..]
  - Build/tooling impact: [BUILD-..]
  - Testing impact: [TEST-..]

#### Alternatives Considered
- **Alt A:** <summary> — rejected because <reason>
- **Alt B:** <summary> — rejected because <reason>

#### Consequences
**Positive:**
- + …
- + …

**Negative / Trade-offs:**
- - …
- - …

**Risks:**
- Risk 1: <description> — mitigation: <plan or follow-up CR>
- Risk 2: <description> — mitigation: <plan or follow-up CR>

#### Verification
How do we know this decision is correct?
- Tests/benchmarks: [TEST-..]
- Quality gates impacted: (sanitizers, perf thresholds, etc.)
- Observable metrics: (latency, memory, throughput, etc.)

#### Notes / Addendum (optional)
Use this section for later clarifications without rewriting history.

---

## 5) Status & Supersession Guidance
### 5.1 Superseded decisions
When replacing a decision:
1) Create a new `[DEC-XX]` entry describing the new choice.
2) In the new entry, set **Supersedes** to the old DEC.
3) In the old entry, set **Superseded By** (allowed minimal edit) or add an Addendum linking forward.

### 5.2 Deprecations
If a decision introduces deprecation policies or removal targets, link to:
- [VER-04] deprecation rules
- Migration documentation requirements (e.g., `MIGRATION.md`)
- Any affected [API-XX] or public symbols

---

## 6) Example (Brief)
### [DEC-01] Use PIMPL for stable public ABI surface
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0
- **Related CRs:** [CR-0001]
- **Scope:** core/public API

#### Context
Need to reduce ABI breakage risk for downstream consumers.
- Constraints: [VER-03], [API-02]

#### Decision
Use PIMPL for public classes and keep public structs POD-only.

#### Alternatives Considered
- Alt A: expose full class layout — rejected due to frequent ABI breaks
- Alt B: C-only API — rejected due to ergonomics and C++-only features needed

#### Consequences
Positive: + ABI churn reduced  
Negative: - slight indirection cost  

#### Verification
- ABI check in CI (if configured), plus downstream integration test.

---

## 7) Maintenance Checklist
- Add new DEC for every significant choice and whenever rationale matters.
- Keep entries concise but complete.
- Ensure every decision references impacted IDs.
- Avoid duplicate DEC entries for the same decision; supersede instead.
