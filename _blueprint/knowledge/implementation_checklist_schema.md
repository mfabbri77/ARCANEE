<!-- implementation_checklist_schema.md -->

# Implementation Checklist Schema (Normative)

This file defines the **required YAML schema** for:
- `/_blueprint/project/implementation_checklist.yaml` (baseline checklist)
- `/_blueprint/vM.m/checklist_delta.yaml` (overlay delta checklist)

The checklist is a **contractual execution plan**: every requirement (**REQ-xxxxx**) MUST be implementable via at least one checklist step, and every step MUST be traceable to requirements/decisions/tests/gates.

This schema is **normative** and must be enforced by the composer/validator (COMP-01).

---

## 1. File invariants

- Checklist files MUST be valid UTF-8 YAML.
- Tabs are forbidden; use spaces.
- Top-level MUST be a mapping with required keys described in §2.
- IDs referenced MUST exist in the effective blueprint (Eff(V)) or be introduced in the same overlay.
- The literal string `N/A` is forbidden.

---

## 2. Top-level schema

### 2.1 Required keys

```yaml
schema_version: "1.0"
checklist_id: "CL-<name-or-version>"
scope:
  applies_to: "baseline|overlay"
  version: "M.m.p|unspecified"
metadata:
  owner: "<role/team>"
  last_updated: "YYYY-MM-DD"
  profile: "PROFILE_GENERAL|PROFILE_AAA_HFT"
  mode: "A|B|C|unknown"
sections:
  - id: "CLSEC-00001"
    title: "<section title>"
    intent: "<short description>"
    items:
      - id: "CL-00001"
        title: "<step title>"
        priority: "P0|P1|P2|P3"
        status: "todo|in_progress|blocked|done"
        rationale: "<why this step exists>"
        trace:
          requirements: ["REQ-00001"]
          decisions: ["DEC-00001"]
          tests: ["TST-00001"]
          gates: ["BUILD-00001"]
        instructions:
          - type: "command"
            shell: "bash|pwsh|cmd"
            run: "<exact command>"
          - type: "manual"
            text: "<human instruction>"
        artifacts:
          produces:
            - path: "/relative/path"
              description: "<what is produced>"
          modifies:
            - path: "/relative/path"
              description: "<what changes>"
        acceptance:
          - "<objective criteria>"
        rollback:
          - "<how to revert if needed>"
        notes:
          - "<optional notes>"
```

### 2.2 Type rules

- `schema_version`: string; MUST be `"1.0"` unless a DEC + validator update introduces new version.
- `checklist_id`: string; SHOULD be stable across baseline; overlay may append version.
- `scope.applies_to`: `"baseline"` for `implementation_checklist.yaml`, `"overlay"` for `checklist_delta.yaml`.
- `scope.version`:
  - baseline: `"unspecified"` or `"0.0.0"`
  - overlay: MUST be the same version as `release.md` (M.m.p)
- `metadata.profile`: MUST match Ch0 profile.
- `metadata.mode`: SHOULD reflect Mode A/B/C once known.
- `sections`: non-empty list.

---

## 3. Section schema

Each section entry MUST contain:

- `id`: string, MUST match `^CLSEC-\d{5}$`
- `title`: non-empty string
- `intent`: non-empty string
- `items`: non-empty list of checklist items

Sections SHOULD be ordered logically:
1) Validation & setup
2) Architecture decisions & contracts
3) Core implementation
4) Tests
5) CI gates & tooling
6) Docs & release

---

## 4. Item schema

### 4.1 Required fields

Each item MUST include:

- `id`: string matching `^CL-\d{5}$`
- `title`: string
- `priority`: `P0|P1|P2|P3`
- `status`: `todo|in_progress|blocked|done`
- `rationale`: string
- `trace`: mapping (see §5)
- `instructions`: non-empty list
- `artifacts`: mapping (produces/modifies; may be empty lists)
- `acceptance`: non-empty list
- `rollback`: list (may be empty)
- `notes`: list (may be empty)

### 4.2 Instruction schema

Each `instructions[]` entry MUST have:

- `type`: `"command"` or `"manual"`

If `type: command`, MUST include:
- `shell`: `"bash"|"pwsh"|"cmd"`
- `run`: exact command line, copy/paste runnable

If `type: manual`, MUST include:
- `text`: explicit human steps; SHOULD include filenames

### 4.3 Artifact schema

`artifacts` MUST contain:
- `produces`: list (may be empty)
- `modifies`: list (may be empty)

Each artifact entry MUST include:
- `path`: repo-root-relative path (start with `/`)
- `description`: string

### 4.4 Acceptance criteria

`acceptance[]` entries MUST be objective and verifiable.
At least one acceptance entry SHOULD reference a test or gate by ID via `trace`.

---

## 5. Traceability requirements (normative)

### 5.1 Minimum trace

Each checklist item MUST reference at least one of:
- a requirement (`REQ-xxxxx`), or
- a decision (`DEC-xxxxx`), or
- a build/test gate (`BUILD-xxxxx`/`TST-xxxxx`).

### 5.2 Global requirement coverage

For the effective blueprint Eff(V):

- Every `REQ-xxxxx` MUST appear in at least one checklist item’s `trace.requirements[]`.
- Every `REQ-xxxxx` MUST have ≥1 test (`TST-xxxxx`) and ≥1 checklist step (enforced by validator).

### 5.3 Decision enforcement coverage

For every accepted decision:
- The checklist SHOULD include at least one step ensuring enforcement exists (tests/gates implemented and run).

---

## 6. Overlay delta checklist rules

`checklist_delta.yaml` MUST:
- include only **new or modified** checklist steps, and/or
- explicitly mark removals via a dedicated `removed_items` list (see below),
- reference the overlay version.

### 6.1 Removal schema (optional but recommended)

```yaml
removed_items:
  - id: "CL-00042"
    reason: "Superseded by CL-00110 due to new tooling"
    replaced_by: "CL-00110"
```

Validator SHOULD ensure removed items exist in prior Eff(V).

---

## 7. Validator requirements (COMP-01 linkage)

The composer/validator MUST validate:
- YAML parse correctness
- schema_version match
- required keys and types
- ID format correctness
- uniqueness of all `CLSEC-*` and `CL-*` IDs within Eff(V)
- referenced IDs exist (REQ/DEC/TST/BUILD)
- global requirement coverage rule (§5.2)

---

## 8. Widely recognized defaults (recommended content guidance)

Baseline checklist SHOULD include early steps to:
- run composer/validator locally,
- configure/build/test via presets on each OS,
- run formatting/lint tools,
- run sanitizer jobs (where feasible),
- execute unit/integration tests,
- run benchmarks when performance budgets exist,
- verify release.md invariants for overlays.

---

## 9. Example minimal baseline checklist

```yaml
schema_version: "1.0"
checklist_id: "CL-baseline"
scope:
  applies_to: "baseline"
  version: "unspecified"
metadata:
  owner: "Architecture"
  last_updated: "2025-01-01"
  profile: "PROFILE_GENERAL"
  mode: "unknown"
sections:
  - id: "CLSEC-00001"
    title: "Validation and Setup"
    intent: "Ensure local environment can run deterministic builds and validators."
    items:
      - id: "CL-00001"
        title: "Run blueprint composer/validator"
        priority: "P0"
        status: "todo"
        rationale: "Guarantees blueprint schema and enforcement rules are satisfied."
        trace:
          requirements: []
          decisions: []
          tests: []
          gates: ["BUILD-00001"]
        instructions:
          - type: "command"
            shell: "bash"
            run: "python3 tools/blueprint/compose_validate.py --current"
        artifacts:
          produces: []
          modifies: []
        acceptance:
          - "Validator exits with status 0 and reports Eff(vCURRENT) valid."
        rollback: []
        notes:
          - "Use `--help` for options."
```
