<!-- walkthrough_template.md -->

# Walkthrough Template (Normative)

This template defines the required structure for:
- `/_blueprint/project/walkthrough.md`
- `/_blueprint/vM.m/walkthrough_delta.md` (delta-specific, see §6)

Walkthroughs are **execution guides** for implementers:
- what to build,
- where to start,
- how to validate,
- how to progress through the checklist deterministically.

This template is **normative** unless overridden via DEC + enforcement.

---

## 1. Baseline walkthrough (`walkthrough.md`) structure (MUST)

### 1.1 Title and purpose

The file MUST begin with:
- `#` title
- a short purpose statement

### 1.2 Table of contents (recommended)

Include links to major sections for easy navigation.

### 1.3 Required sections (MUST)

The walkthrough MUST include these sections in order.

#### 1) Overview
- What the system is
- High-level goals and constraints
- Active profile (`PROFILE_GENERAL` or `PROFILE_AAA_HFT`)
- Where the SoT blueprint lives (`/_blueprint/`)

#### 2) Quickstart (Local)
Provide exact commands to:
- install prerequisites
- configure/build/test using presets
- run blueprint validator

Commands MUST be copy/paste runnable and match Ch7.

#### 3) Repo orientation
- Map key directories to responsibilities:
  - `/src`, `/include`, `/tests`, `/tools`, `/docs`, `/_blueprint`
- Point to key blueprint chapters and how they relate.

#### 4) Architecture in 10 minutes
A condensed summary that links to:
- Ch2 C4 overview
- Ch3 components
- Ch4 API/ABI
- Ch6 concurrency
- Ch5 hotpath (if perf-sensitive)

#### 5) Contracts you must not break
List the most important contracts with IDs:
- key DEC decisions
- key REQ requirements
- key APIs
- determinism/performance budgets if applicable

#### 6) Implementation plan
Explain how to execute `implementation_checklist.yaml`:
- how items are grouped
- what to do first
- how to mark progress (status fields)
- how to validate each milestone

#### 7) Testing and CI
- How to run unit/integration tests locally
- How CI gates map to local commands
- Sanitizers/analysis jobs and how to run them locally

#### 8) Debugging and troubleshooting
- Common failure modes (build issues, missing presets, validator errors)
- How to interpret gate failures (include examples of messages)
- TEMP debug policy reminder (link to temp_dbg_policy.md)

#### 9) Release and versioning
- Where release notes live (overlay `release.md`)
- SemVer interpretation (link to Ch9)
- How to prepare a release (gates, CRs)

#### 10) Appendix
- Tool versions
- Links to important policies (dependency, logging, error handling, testing)
- Glossary of key terms and IDs

---

## 2. Walkthrough content rules (MUST)

### 2.1 Command accuracy

All commands MUST be:
- deterministic,
- consistent with Ch7 toolchain/presets,
- OS-specific where needed (`bash` vs `pwsh`).

### 2.2 Cross-platform guidance

When commands differ by OS:
- provide separate subsections for Linux/macOS vs Windows
- avoid ambiguous instructions like “run the build”.

### 2.3 ID references

When referencing requirements/decisions/tests/gates:
- include ID and short title
- link to relevant blueprint sections when possible

### 2.4 Avoid duplication of SoT

The walkthrough MAY summarize, but MUST not become a second source of truth that contradicts chapters.  
If there is a conflict, chapters win (SoT). Fix via CR/DEC.

---

## 3. Suggested “Quickstart” commands (informative defaults)

Linux/macOS:

```bash
cmake --preset ci-linux
cmake --build --preset ci-linux
ctest --preset ci-linux --output-on-failure
python3 tools/blueprint/compose_validate.py --current
```

Windows (PowerShell):

```pwsh
cmake --preset ci-windows
cmake --build --preset ci-windows
ctest --preset ci-windows --output-on-failure
python tools/blueprint/compose_validate.py --current
```

(Exact preset names may vary; document via DEC if different.)

---

## 4. Example “Contracts you must not break” section (informative)

Include bullet list like:

- REQ-00010 — Cross-platform build must pass on ubuntu/windows/macos
- DEC-00005 — Public APIs do not throw exceptions (PROFILE_AAA_HFT)
- API-00120 — `OrderRouter::submit(...)` is thread-safe and lock-free on hotpath
- BUILD-00001 — blueprint validator must pass; no `N/A`, no `[TEMP-DBG]`

---

## 5. Baseline file checklist (recommended)

Walkthrough SHOULD link to:
- `/_blueprint/project/decision_log.md`
- `/_blueprint/project/implementation_checklist.yaml`
- `/_blueprint/project/ch7_build_toolchain.md`
- `/_blueprint/rules/*` key policies

---

## 6. Delta walkthrough (`walkthrough_delta.md`) rules (MUST)

For overlay delta walkthroughs:
- The file MUST focus on:
  - what changed in this version
  - what implementers must do differently
  - migration steps
  - new/changed gates and tests

### 6.1 Required sections for delta

1) Delta summary (what changed)
2) Impact matrix (components/APIs/build/test)
3) Migration steps (ordered)
4) New/changed gates (how to pass)
5) Deprecated behaviors/APIs
6) Rollback plan (if applicable)
7) Links to CRs and decision deltas

Delta walkthrough MUST NOT restate unchanged baseline content; link to baseline instead.

---

## 7. Policy changes

Changes to this template MUST be introduced via CR and MUST include:
- migration notes for existing walkthroughs,
- validator updates if any required headings change.
