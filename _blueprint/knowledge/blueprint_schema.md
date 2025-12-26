<!-- blueprint_schema.md -->

# Blueprint Schema (DET CDD ARCH AGENT)

This document defines the **canonical, machine-validatable schema** for all blueprint artifacts under `/_blueprint/`.
It is written for both humans and automation (composer/validator, CI gates) and is **normative** unless explicitly marked “Non‑normative”.

---

## 1. Purpose

The blueprint system is a **contract-driven architecture specification** designed to:
- transform a spec or repo into a deterministic, testable architecture baseline (v1.0 → v2.0+),
- make all ambiguity explicit via **DEC** items,
- ensure every requirement is **enforced** by tests and/or CI gates,
- support versioned evolution via **SemVer** overlays.

---

## 2. Definitions and terminology

### 2.1 Canonical terms

- **Blueprint**: the entire content under `/_blueprint/` including `project/` baseline and version overlays `vM.m/`.
- **SoT (Source of Truth)**: the `/_blueprint/` directory.
- **Eff(V)** (“Effective Blueprint at Version V”): the result of composing `project/` with all overlays up to and including V.
- **Overlay**: a version directory `/_blueprint/vM.m/` containing delta artifacts.
- **Gate**: a CI-enforced rule that fails the build when violated.
- **Contract**: a binding definition of subsystem behavior (API, lifetimes, threading, errors, determinism, performance), validated by tests and gates.
- **Mode**: input classification A/B/C (new project, migrate/repair, upgrade/delta). Mode selection is governed by separate policy files, but artifacts must remain schema-compliant.

### 2.2 Normative language

This document uses RFC 2119 / RFC 8174 keywords:
- **MUST / MUST NOT**
- **SHOULD / SHOULD NOT**
- **MAY**

### 2.3 Chapter model

The blueprint is organized as **Ch0..Ch9**:
- Ch0 META
- Ch1 Scope + Requirements + Budgets
- Ch2 C4 + Platform Matrix + Repo Map + Packaging
- Ch3 Components
- Ch4 Interfaces / API / ABI
- Ch5 Data + Hotpath
- Ch6 Concurrency
- Ch7 Build / Toolchain
- Ch8 Tooling
- Ch9 Versioning / Lifecycle

All chapters MUST exist in Eff(V). Each chapter file is a Markdown file.

---

## 3. Repository layout (blueprint-visible)

Blueprint schema assumes these top-level repo directories exist (or are created by migration):
- `/_blueprint/` (SoT)
- `/src`, `/include`, `/tests`, `/docs`, `/tools`, `/examples`
- `/CMakeLists.txt`

`/_blueprint/` MUST contain:
- `/_blueprint/_knowledge/` (read-only reference knowledge files)
- `/_blueprint/rules/` (enforceable rule files used by CI and authoring)
- `/_blueprint/project/` (baseline blueprint: Ch0..Ch9 + supporting files)
- `/_blueprint/vM.m/` (one or more overlay versions)

---

## 4. Global file invariants

### 4.1 Markdown filename header (CI‑enforced)

Every produced `*.md` under `/_blueprint/` MUST begin with exactly one first line comment:
- First line MUST be exactly: `<!-- filename.md -->`
- Where `filename.md` is the file’s basename (case-sensitive).
- No other content is allowed on line 1.
- Line 2 MAY be blank.

**Examples**
- ✅ `<!-- ch3_components.md -->`
- ❌ `<!-- /_blueprint/project/ch3_components.md -->` (path forbidden)
- ❌ `<!-- CH3_components.md -->` (case mismatch)
- ❌ Any YAML front-matter (forbidden; breaks invariant)

### 4.2 No “N/A” in blueprint content

The literal string `N/A` is forbidden anywhere in `/_blueprint/**` Markdown or YAML unless a rule file explicitly allows it.
Use a **DEC** item instead (see §7).

### 4.3 “Inherit” exactness (overlay chapters)

If a version overlay includes an unchanged chapter file, it MUST contain exactly one non-empty line:
- `inherit from <relpath>`

Where `<relpath>` is a repository-relative path pointing to the prior effective artifact (e.g., `/_blueprint/project/ch3_components.md` or `/_blueprint/v1.2/ch3_components.md`).

CI MUST fail if:
- `inherit from ...` is missing,
- the target does not exist,
- the target cannot be read,
- the file contains any other content.

### 4.4 Allowed content formats

- Markdown (`*.md`) for narrative/spec chapters and templates.
- YAML (`*.yaml`) for checklists and machine-readable structures.
- JSON (`CMakePresets.json` etc.) for build metadata (outside `/_blueprint/` unless a rule says otherwise).

---

## 5. ID system schema

### 5.1 Purpose

IDs provide:
- stable referencing of requirements, decisions, tests, gates, rules, and versions,
- monotonic allocation ensuring no reuse,
- diff stability across upgrades.

### 5.2 ID format (normative)

IDs MUST match:

- **Standard IDs** (REQ/DEC/TST/etc.)
  - Pattern: `PREFIX-NNNNN`
  - `PREFIX` is one of:
    - `META`, `SCOPE`, `REQ`, `ARCH`, `DEC`, `MEM`, `CONC`, `API`, `BUILD`, `TST`, `VER`, `TEMP-DBG`
  - `NNNNN` is a zero-padded decimal integer (5 digits).

- **Change Requests**
  - Pattern: `CR-XXXX` (4 digits, zero padded).

**Regexes**
- Standard: `^(META|SCOPE|REQ|ARCH|DEC|MEM|CONC|API|BUILD|TST|VER|TEMP-DBG)-\d{5}$`
- CR: `^CR-\d{4}$`

### 5.3 Monotonic allocation rules

- IDs are monotonic **per-prefix per emission**.
- Never renumber or reuse IDs.
- Allocation start:
  - Determine `N = max numeric suffix across /_blueprint/**/*.{md,yaml} + 1` for that prefix.
  - If no prior IDs exist for that prefix, start at `00001`.

### 5.4 ID referencing rules

Any artifact referencing an ID MUST:
- refer to the exact ID string (case-sensitive),
- include enough local context to resolve the reference without searching (e.g., requirement title or short name).

---

## 6. Core blueprint artifacts and their schema

### 6.1 Baseline blueprint directory: `/_blueprint/project/`

Required files (baseline):
- `ch0_meta.md`
- `ch1_scope_requirements.md`
- `ch2_architecture_overview.md`
- `ch3_components.md`
- `ch4_interfaces_api_abi.md`
- `ch5_data_hotpath.md`
- `ch6_concurrency.md`
- `ch7_build_toolchain.md`
- `ch8_tooling.md`
- `ch9_versioning_lifecycle.md`
- `decision_log.md`
- `walkthrough.md`
- `implementation_checklist.yaml`

### 6.2 Overlay directory: `/_blueprint/vM.m/`

Required files (overlay):
- `release.md`
- `decision_delta_log.md`
- `walkthrough_delta.md`
- `checklist_delta.yaml`
- `CR-XXXX.md` (one or more, referenced in `release.md`)
- Any changed `ch*.md` files (only when changed; otherwise omit or use strict inherit file per §4.3)

**Naming constraint**
- Overlay directories represent a **minor line**: `vM.m` (e.g., `v1.3`).
- Patch releases are recorded in `release.md` history; creating a `vM.m.p/` directory is forbidden unless explicitly allowed by decision + gate.

---

## 7. Decision schema (DEC)

### 7.1 When a DEC is required

A **DEC** item MUST be created when:
- input is ambiguous or unknown,
- a rule file or tag map is missing or unclear,
- a tradeoff exists with multiple viable alternatives,
- constraints conflict and need arbitration,
- a non-default or risky choice is made (packaging exceptions, GPU backend selection, OOS tooling, etc.).

### 7.2 DEC content structure (normative)

A DEC item MUST include:

- **ID**: `DEC-xxxxx`
- **Title**: short imperative description
- **Status**: `proposed | accepted | superseded | rejected`
- **Context / Why**: what is unknown/ambiguous and why a choice is needed
- **Options**: at least 2 options unless truly binary
  - each option includes Pros / Cons / Consequences
- **Decision**: chosen option + rationale
- **Enforcement**: one or more of:
  - **Rule(s)** (normative statements),
  - **TST references** (≥1 where applicable),
  - **CI gate(s)** (≥1 if automation is feasible),
- **Migration / Compatibility Notes** (if decision affects compatibility or versioning)
- **Links**: to impacted REQ/ARCH/API items by ID

### 7.3 DEC formatting (recommended block)

Use this Markdown block style for consistency:

```md
## DEC-00001 — <title>
**Status:** accepted  
**Context:** ...  
**Options:**  
- A) ... Pros: ... Cons: ... Consequences: ...
- B) ... Pros: ... Cons: ... Consequences: ...
**Decision:** ...
**Enforcement:**  
- Rule: ...
- TST: TST-00012, TST-00013
- Gate: CI-GATE-... (if your gates are separately ID’d, use BUILD- or TST- prefixed IDs)
**Migration/Compat:** ...
**Impacts:** REQ-..., API-..., ARCH-...
```

---

## 8. Requirement schema (REQ)

### 8.1 Minimum required fields

Each requirement MUST have:
- **REQ ID** (`REQ-xxxxx`)
- **Title**
- **Type**: `functional | nonfunctional | constraint | regulatory | operational`
- **Priority**: `P0 | P1 | P2 | P3`
- **Rationale**
- **Acceptance criteria** (testable)
- **Traceability**
  - at least 1 test: `TST-xxxxx`
  - at least 1 checklist step referencing the requirement ID

### 8.2 Recommended requirement block

```md
### REQ-00042 — <title>
**Type:** nonfunctional  
**Priority:** P0  
**Rationale:** ...  
**Acceptance Criteria:**  
- ... (objective, measurable)  
**Traceability:**  
- Tests: TST-00110  
- Checklist: CL step “...” in implementation_checklist.yaml
```

---

## 9. Test schema (TST)

### 9.1 Test types

Supported test types (use one or more):
- `unit`
- `integration`
- `property`
- `stress`
- `negative`
- `fuzz`
- `performance`

### 9.2 Minimum required fields

Each test entry MUST have:
- **TST ID** (`TST-xxxxx`)
- **Type**
- **Scope**
- **Tooling** (framework, runner)
- **Command(s)** (exact CLI)
- **Pass/Fail criteria**
- **Traceability** to REQ and/or DEC

---

## 10. CI gate schema (BUILD/TST gates)

### 10.1 Gate definition requirements

Every CI gate MUST be:
- uniquely identifiable (recommended: `BUILD-xxxxx` or `TST-xxxxx` depending on nature),
- deterministic (no flaky external dependencies without explicit isolation),
- runnable locally via documented command(s),
- referenced from:
  - Ch7 Build/Toolchain and/or
  - release.md gates list.

### 10.2 Examples of normative gates (must exist as policies elsewhere)

Blueprint content MUST support gates such as:
- no `[TEMP-DBG]` left in repo
- no `N/A` occurrences
- all REQ have ≥1 TST and ≥1 checklist step
- markdown header comment exists and matches filename
- inheritance resolves
- SemVer invariants are satisfied
- xplat CI matrix exists and runs build+test on ubuntu/windows/macos
- sanitizer/analysis jobs exist per platform feasibility

(Exact gate implementations are defined in `ci_reference.md` and related policy files.)

---

## 11. Chapter schemas (Ch0..Ch9)

Each chapter MUST follow:
- a top-level `#` title,
- subsections with stable headings,
- ID usage for any normative statements (REQ/DEC/ARCH/API/etc.),
- explicit cross-references (by ID).

Below are **minimum required sections** per chapter.

### 11.1 Ch0 — META (`ch0_meta.md`)

MUST include:
- **META-xxxxx** entries for meta rules and invariants in force
- Active profile: `PROFILE_AAA_HFT` or `PROFILE_GENERAL`
- Mode Evidence summary (A/B/C) when applicable
- Artifact inventory (files expected in project and current overlays)

### 11.2 Ch1 — Scope + Requirements + Budgets (`ch1_scope_requirements.md`)

MUST include:
- In-scope / out-of-scope
- Non-goals
- Stakeholders and constraints
- **REQ list** with traceability hooks
- Budgets: latency, throughput, memory, CPU, startup, log overhead, determinism (if AAA/HFT)
- Observability policy application statement (links to knowledge file; enforcement via tests/gates)

### 11.3 Ch2 — Architecture overview (`ch2_architecture_overview.md`)

MUST include:
- C4 model diagrams list (System Context, Container, Component at minimum; code-level optional)
- Platform matrix: OS/arch/compiler/stdlib baselines
- Repo map & packaging decisions (packaging exceptions require DEC+gate)

### 11.4 Ch3 — Components (`ch3_components.md`)

MUST include:
- Component list with responsibilities and boundaries
- Dependency direction rules
- Performance-critical component identification (hotpath ownership)

### 11.5 Ch4 — Interfaces / API / ABI (`ch4_interfaces_api_abi.md`)

MUST include:
- Public API surfaces (headers, libs, namespaces)
- Ownership & lifetimes
- Threading and synchronization contract per API
- Error handling model
- ABI policy per platform (macros, calling conventions, exceptions/RTTI)
- API/ABI compatibility strategy mapped to SemVer rules

### 11.6 Ch5 — Data + Hotpath (`ch5_data_hotpath.md`)

MUST include:
- Data model(s), serialization if any
- Hotpath description, perf assumptions, allocator strategy
- Instrumentation boundaries (what is allowed on hotpath)
- Benchmark hooks (IDs referencing perf harness)

### 11.7 Ch6 — Concurrency (`ch6_concurrency.md`)

MUST include:
- Thread model and scheduling assumptions
- Shared state inventory + synchronization primitives
- Determinism strategy (time sources, ordering guarantees, randomness control) when applicable
- Contention avoidance and backpressure strategy

### 11.8 Ch7 — Build / Toolchain (`ch7_build_toolchain.md`)

MUST include:
- Toolchain pinning: compilers, standard, CMake minimum, generators
- Presets: required `CMakePresets.json` and per-OS presets
- Exact commands to configure/build/test (local + CI)
- Invocation of composer/validator script (COMP-01)

### 11.9 Ch8 — Tooling (`ch8_tooling.md`)

MUST include:
- Format/lint/static analysis tools
- Sanitizers/analysis jobs per platform
- Developer workflow steps and IDE integration (optional, but bounded by gates)

### 11.10 Ch9 — Versioning / Lifecycle (`ch9_versioning_lifecycle.md`)

MUST include:
- SemVer rules: what constitutes MAJOR/MINOR/PATCH
- Deprecation policy
- Migration policy
- Performance regression policy
- Release process with gate list
- Version history links to overlay release.md files

---

## 12. Release schema (`release.md` in overlays)

Each `/_blueprint/vM.m/release.md` MUST include:
- **Version** (M.m.p)
- **Previous** (previous patch in same line or previous line as applicable)
- **Compatibility Notes** (deprecations, migrations, perf)
- **CR list** (CR IDs)
- **Changed artifacts** list
- **Gates list** (IDs and/or concrete job names)
- **Release History** immutable table (append-only)

CI invariants:
- Version strictly increases compared to latest Release History entry.
- Previous matches immediate prior patch (or prior line if first patch in a minor line).
- CR IDs referenced exist as files.

---

## 13. Composer / Validator expectations (COMP-01)

A repo MUST ship a CI-invoked script (location defined by Ch7) that:
1. Reconstructs **Eff(vCURRENT)** and (if applicable) **Eff(vNEXT)**.
2. Validates:
   - all Ch0..Ch9 exist,
   - no forbidden literals (`N/A`, `[TEMP-DBG]` leftovers, etc.),
   - all `inherit from ...` resolves,
   - all referenced rule files exist or have a DEC + failing-fast gate,
   - all REQ have ≥1 TST and ≥1 checklist step,
   - version/release invariants.

The validator MUST fail hard (non-zero exit) on any violation.

---

## 14. Agent authoring guidelines (non‑normative, recommended)

- Prefer **enforcement** (tests/gates) over prose.
- Convert ambiguity into DEC items quickly; do not speculate silently.
- Keep APIs minimal and explicit; specify ownership, lifetimes, threading, and errors.
- Use C4 naming consistently for diagrams and model elements.
- Use CMake Presets to encode reproducible configurations.

---

## 15. Reference links (non‑normative)

For convenience, canonical references used by defaults in this ecosystem:

```text
SemVer 2.0.0: https://semver.org/
CMake Presets manual: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
C4 model: https://c4model.com/
GitHub Actions matrix: https://docs.github.com/actions/writing-workflows/choosing-what-your-workflow-does/running-variations-of-jobs-in-a-workflow
```
