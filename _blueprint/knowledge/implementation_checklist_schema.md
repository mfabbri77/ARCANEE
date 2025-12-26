<!-- _blueprint/knowledge/implementation_checklist_schema.md -->

# Implementation Checklist Schema (Normative)
This document defines the canonical **YAML schema** for `implementation_checklist.yaml` produced at the end of every Blueprint.  
It is designed to be **machine-parseable**, enforce referential integrity to Blueprint IDs, and drive AI coding agents deterministically.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) File Location & Naming
- File path: `/blueprint/implementation_checklist.yaml`
- Top-level key MUST be exactly: `implementation_checklist`

**Rule:** This file must be regenerated whenever the Blueprint changes in a meaningful way (typically via a CR).

---

## 2) Required Structure (Canonical)
The YAML must conform to this structure:

```yaml
implementation_checklist:
  blueprint_version: "X.Y"
  repo_name: "<repo>"
  milestones:
    - id: "MS-01"
      name: "..."
      steps:
        - id: "TASK-01.01"
          action: "..."
          refs: ["REQ-..", "ARCH-..", "DEC-..", "MEM-..", "CONC-..", "API-..", "PY-..", "BUILD-..", "TEST-..", "VER-.."]
          artifacts: ["path/or/target", "..."]
          verify: ["command ...", "..."]
          notes: "optional"
  quality_gates:
    - id: "GATE-01"
      name: "..."
      command: "..."
      refs: ["BUILD-..", "TEST-..", "TEMP-DBG"]
  release:
    tagging: "v<MAJOR>.<MINOR>.<PATCH>"
    required_files: ["CHANGELOG.md", "MIGRATION.md"]
    required_gates: ["GATE-01", "GATE-02"]
    build_artifacts:
      - name: "..."
        command: "..."
        outputs: ["path", "..."]
```

---

## 3) Field Definitions (Normative)
### 3.1 `blueprint_version` (required)
- String, format `MAJOR.MINOR` (e.g., `"1.0"`, `"2.1"`)
- Must match `[META-01]` in the Blueprint snapshot.

### 3.2 `repo_name` (required)
- Short repository name (no spaces preferred).

### 3.3 `milestones[]` (required, non-empty)
Each milestone groups steps into a coherent deliverable.

Fields:
- `id` (required): `MS-XX` where XX is 2 digits (`01..99`)
- `name` (required): human-friendly label
- `steps` (required, non-empty): list of step items

### 3.4 `steps[]` (required, non-empty)
Fields:
- `id` (required): `TASK-XX.YY` where XX=milestone number, YY=step number, both 2 digits  
  Example: `TASK-02.03`
- `action` (required): imperative instruction that yields concrete repo changes
- `refs` (required, non-empty): list of Blueprint IDs or tags (see §4)
- `artifacts` (required, non-empty): list of created/modified artifacts
  - file paths (e.g., `CMakePresets.json`, `src/foo.cpp`)
  - targets (e.g., `target:<proj>`, `target:<proj>_tests`)
  - docs (e.g., `docs/ARCHITECTURE.md`)
- `verify` (required, non-empty): runnable commands that validate the step
- `notes` (optional): clarifications; should not contain new requirements

**Rule:** Every step must be verifiable.

### 3.5 `quality_gates[]` (required, non-empty)
Fields:
- `id` (required): `GATE-XX`
- `name` (required)
- `command` (required): enforceable command/target that fails on violation
- `refs` (required): must include relevant Blueprint IDs; include `TEMP-DBG` for the debug gate

**Rule:** Gates are blocking for merge/release unless explicitly declared “non-blocking” in the Blueprint (discouraged).

### 3.6 `release` (required)
Fields:
- `tagging` (required): string pattern, typically `v<MAJOR>.<MINOR>.<PATCH>`
- `required_files` (required):
  - must include `CHANGELOG.md`
  - must include `MIGRATION.md` when MAJOR releases are possible
- `required_gates` (required): list of gate IDs that must pass for release
- `build_artifacts` (optional): list of artifacts to produce (packages, wheels, SDK zip)

---

## 4) `refs` Rules (Referential Integrity)
### 4.1 Allowed refs
Each entry in `refs` must be either:
- a Blueprint ID (e.g., `REQ-04`, `API-02`, `CONC-01`) **without brackets** OR
- the special marker `TEMP-DBG` OR
- a CR id (e.g., `CR-0042`) if the step implements a CR item

**Rule:** Do not include the square brackets in YAML refs. Use `REQ-04`, not `[REQ-04]`.

### 4.2 Existence requirement
Every referenced ID must exist in the current Blueprint snapshot (or be an approved special tag).

If the Blueprint is updated, update refs accordingly.

---

## 5) Verification Command Rules
- Commands must be runnable in a clean checkout using documented prerequisites.
- Prefer preset-based commands:
  - `cmake --preset dev`
  - `cmake --build --preset dev`
  - `ctest --preset dev --output-on-failure`
- For gates, prefer explicit targets:
  - `cmake --build --preset dev --target check_no_temp_dbg`
  - `cmake --build --preset dev --target format_check`

**Rule:** Avoid vague verification like “run tests” without specifying commands.

---

## 6) Minimal Required Quality Gates (Recommended baseline)
At minimum, include gates for:
- No TEMP-DBG markers:
  - `check_no_temp_dbg`
- Formatting clean:
  - `format_check`
- Tests pass:
  - `ctest --preset <preset>`
- Sanitizers (at least ASan and UBSan on Linux):
  - `ctest --preset asan`
  - `ctest --preset ubsan`

Projects may add:
- lint (clang-tidy)
- benchmark regression checks
- packaging/install consumer test

---

## 7) Evolution Across Versions
When moving from v1.0 to v2.0:
- Create a CR describing the change set.
- Update Blueprint snapshot (`blueprint_v2.0.md`) and decision log as needed.
- Regenerate checklist:
  - new milestones/steps for new features
  - new gates if the change introduces new risk areas

**Rule:** Checklist is always aligned to the current Blueprint snapshot and target release.

---

## 8) Compliance Checklist
- [ ] Top-level key is `implementation_checklist`
- [ ] All milestones, steps, and gates have correct IDs
- [ ] `refs` contains only valid IDs (no brackets)
- [ ] Every step has artifacts and verify commands
- [ ] Gates are enforceable and used by CI
- [ ] Release section includes required files and required gates
