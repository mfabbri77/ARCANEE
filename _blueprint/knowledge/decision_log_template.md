<!-- decision_log_template.md -->

# Decision Log Template (Normative)

This file provides the **required structure** for decision logs:
- `/_blueprint/project/decision_log.md` (baseline)
- `/_blueprint/vM.m/decision_delta_log.md` (overlay delta)

Decision logs are the **authoritative record** of architectural and process decisions (**DEC-xxxxx**) that resolve ambiguity, select tradeoffs, and define enforceable rules.

This template is **normative**: decision logs MUST follow this structure unless a higher-precedence rulefile overrides it.

---

## 1. Requirements (MUST)

### 1.1 Stable IDs and immutability constraints

- Every decision MUST have a stable ID `DEC-xxxxx` (see `blueprint_schema.md`).
- IDs MUST NEVER be renumbered, reused, or reassigned.
- Once a decision is **accepted**, its text SHOULD be treated as append-only:
  - corrections MUST be made via:
    - a **superseding decision**, or
    - a **CR** (Change Request) referencing the original decision, with explicit delta notes.

### 1.2 No silent decisions

If ambiguity exists, the blueprint MUST NOT silently assume. Instead:
- create a DEC with options and consequences, and
- define enforcement (tests and/or gates).

### 1.3 Enforcement is mandatory

Each DEC MUST define at least one enforcement mechanism:
- a **rule** (normative statement) and
- at least one of:
  - **TST** reference(s), and/or
  - **CI gate** reference(s).

If enforcement is not currently implementable, the DEC MUST:
- include a failing-fast gate that blocks release/upgrade correctness claims until enforcement exists.

---

## 2. File structure (MUST)

Decision logs MUST follow this order:

1. Title and scope
2. Decision index table(s)
3. Status legend
4. Decisions, in chronological order (by ID allocation sequence)

### 2.1 Title and scope header (MUST)

The decision log MUST begin with:

- A `#` title
- A short paragraph describing scope:
  - baseline: covers entire project baseline
  - delta: covers changes introduced in this overlay version

### 2.2 Decision index tables (MUST)

Provide:
- an **Index by ID** (ascending)
- an **Index by Topic Tag** (optional but recommended)

Index entries MUST include:
- ID
- Title
- Status
- Date (YYYY-MM-DD)
- A short “Impacts” summary (IDs of REQ/ARCH/API/BUILD/TST affected)

**Index by ID example**

| ID | Title | Status | Date | Impacts |
|---|---|---|---|---|
| DEC-00001 | Choose baseline toolchain and standard | accepted | 2025-01-15 | BUILD-00001, REQ-00003 |
| DEC-00002 | Error handling model | accepted | 2025-01-16 | API-00010, REQ-00007 |

### 2.3 Status legend (MUST)

Include this legend verbatim (may extend with additional statuses only via DEC + gate):

- **proposed**: under consideration; not yet binding
- **accepted**: binding; must be enforced by tests/gates
- **superseded**: replaced by a newer decision (must link to superseding DEC)
- **rejected**: considered and explicitly not chosen (document why)

---

## 3. Decision entry schema (MUST)

Each decision entry MUST use the following headings and fields.

### 3.1 Canonical decision block

```md
## DEC-00000 — <Title>
**Status:** proposed|accepted|superseded|rejected  
**Date:** YYYY-MM-DD  
**Tags:** BACKEND:<...> PLATFORM:<...> TOPIC:<...> (optional; if used, follow tag_map.yaml)  

### Context
Explain the ambiguity, constraints, and why a decision is needed.

### Options
- **A) <Option name>**
  - Pros:
  - Cons:
  - Consequences:
- **B) <Option name>**
  - Pros:
  - Cons:
  - Consequences:
(Include more options as needed.)

### Decision
State the chosen option and rationale. If “no decision”, keep status proposed and define next steps + gate.

### Rules (Normative)
List binding rules derived from this decision. Use bullet points; each rule SHOULD be individually testable.

### Enforcement
- **Tests:** TST-xxxxx, ...
- **CI Gates:** BUILD-xxxxx and/or TST-xxxxx and/or explicit job names
- **Validator hooks:** (if applicable) references to composer/validator checks

### Compatibility / Migration / Deprecation
- Compatibility notes (source/ABI/behavior)
- Migration steps (if needed)
- Deprecations introduced (if any)

### Impacts / Traceability
- **Requirements:** REQ-xxxxx, ...
- **Architecture:** ARCH-xxxxx, ...
- **APIs:** API-xxxxx, ...
- **Build/Tooling:** BUILD-xxxxx, ...
- **Other Decisions:** DEC-xxxxx (links)
```

### 3.2 Additional requirements

- **Options** MUST include at least 2 choices unless truly binary. If binary, explicitly state why.
- **Decision** MUST be unambiguous and non-contradictory with existing accepted decisions. If it contradicts, it MUST supersede.
- If **superseded**, the entry MUST include:
  - `**Superseded by:** DEC-xxxxx`
- If **rejected**, it MUST include:
  - `**Rejection reason:** ...`

---

## 4. Delta decision log rules (overlay) (MUST)

For `/_blueprint/vM.m/decision_delta_log.md`:

- Only include decisions that:
  - are new in this overlay, or
  - supersede/modify prior decisions.
- Every supersession MUST include:
  - old DEC ID
  - new DEC ID
  - reason for supersession
  - enforcement changes

Additionally, include a top section:

```md
## Delta Summary
- New decisions: DEC-..., ...
- Superseded: DEC-old → DEC-new
- Rejected: DEC-..., ...
- Enforcement changes: ...
```

---

## 5. Common decision categories (recommended)

These topics commonly require DEC entries:

- Toolchain / platform matrix / C++ standard version
- Error handling model (exceptions vs expected-like vs status codes)
- Threading model and synchronization primitives
- Public API and ABI policy across platforms
- Memory ownership/lifetime model
- Logging/observability budgets and redaction rules
- Determinism controls (time source, scheduling assumptions, FP controls)
- Packaging/distribution and internal-only packaging exceptions
- GPU backend selection and abstraction boundaries (if TOPIC=gpu)
- Dependency policy and vendoring strategy
- Testing strategy and CI matrix (sanitizers, fuzzing, perf gates)
- Release process (SemVer interpretation, migration policies)

---

## 6. Minimal example (copy/paste)

```md
## DEC-00001 — Choose baseline toolchain and C++ standard
**Status:** accepted  
**Date:** 2025-01-15  
**Tags:** PLATFORM:xplat TOPIC:build  

### Context
We need a reproducible cross-platform toolchain and a default language standard compatible with our dependencies.

### Options
- **A) C++20 + CMake 3.28+ + Ninja (default)**
  - Pros: strong compiler support; modern features; good tooling
  - Cons: some older platforms may require upgrades
  - Consequences: minimum OS/compiler baselines must be pinned
- **B) C++17 + CMake 3.20+**
  - Pros: wider legacy support
  - Cons: less expressive; more custom utilities; fewer std features
  - Consequences: higher maintenance burden

### Decision
Choose A) C++20 with CMake presets and Ninja as the primary generator; keep MSBuild via presets on Windows.

### Rules (Normative)
- The repository MUST build with C++20 as the default standard.
- All CI jobs MUST use CMakePresets.json configure/build/test presets.
- Platform matrix MUST include ubuntu/windows/macos jobs.

### Enforcement
- Tests: TST-00010 (configure/build/test via presets)
- CI Gates: BUILD-00001 (xplat matrix), BUILD-00002 (presets required)

### Compatibility / Migration / Deprecation
No breaking API changes. Existing code must be compatible with C++20.

### Impacts / Traceability
- Requirements: REQ-00003
- Build/Tooling: BUILD-00001, BUILD-00002
```

---

## 7. Template usage notes (non-normative)

- Keep decisions short but complete; prefer bullet lists.
- If an option is risky, call it out explicitly and add extra tests/gates.
- When a decision impacts performance budgets or determinism, add benchmark and regression gates.
