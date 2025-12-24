# Blueprint v0.2 — Chapter 9: Versioning, Lifecycle, Governance

## 9.1 SemVer Policy
inherit from /blueprint/blueprint_v0.1_ch9.md

### 9.1.1 v0.2 release target
- [VER-10] **Release target:** ship this CR as **v0.2.0** (MINOR bump from v0.1.0).
  - Consequences:
    - v0.2.0 must include all DoD items from [CR-0043] and pass the new CI gates in Ch7.
    - Any deferral of required IDE features must be done via a follow-up CR that adjusts requirements/tests/checklist (never silent).

## 9.2 Change Request Governance
inherit from /blueprint/blueprint_v0.1_ch9.md

### 9.2.1 v0.2 governance addendum (Mode C discipline)
- [VER-11] **Upgrade governance rule (Mode C):**
  - All v0.2 changes are tracked under [CR-0043] and recorded as delta-only:
    - blueprint chapters use “inherit from …” where unchanged
    - new/changed decisions are recorded in `decision_delta_log.md`
  - Consequences:
    - No renumbering of existing IDs.
    - New IDs are append-only and must be reflected in tests + checklist gates.

## 9.3 Deprecation & Migration Policy
inherit from /blueprint/blueprint_v0.1_ch9.md

### 9.3.1 IDE settings schema evolution
- [VER-12] **TOML schema compatibility**
  - Rules:
    - unknown keys must be preserved when reading/writing (do not delete)
    - add-only schema changes are allowed without migration
    - removals/renames require:
      1) deprecation period (≥1 MINOR release) with compatibility reads
      2) explicit migration notes in walkthrough + checklist
  - Consequences:
    - Settings/keybindings/session files under `.arcanee/` must include a top-level `schema_version = <int>` field.
    - Add tests under [TEST-34] to validate unknown keys survive roundtrip.

### 9.3.2 Timeline schema evolution
- [VER-13] **TimelineStore schema versioning**
  - Rules:
    - SQLite DB includes `PRAGMA user_version` and a `meta(schema_version)` row
    - migrations must be forward-only and idempotent
    - on migration failure: TimelineStore disables itself and logs a single actionable error (editing continues)
  - Consequences:
    - [TEST-31] must include a migration test fixture (v1→v2 when introduced later).
    - Timeline data remains project-local per [DEC-35]; upgrades must not silently wipe user history.

## 9.4 Performance Regression Policy
inherit from /blueprint/blueprint_v0.1_ch9.md

### 9.4.1 IDE budget enforcement
- [VER-14] **Perf regression gate policy (IDE)**
  - Decision: Ch1 budgets from [DEC-34] are enforced by CI micro-bench tests:
    - [TEST-35] typing latency micro-bench
    - [TEST-36] snapshot micro-bench
  - Rules:
    - If a regression is detected on CI reference runners, the PR must either:
      1) fix performance, or
      2) add a CR that updates the budget with rationale + downstream consequences, and adjust tests accordingly.
  - Consequences:
    - “Perf exceptions” are not allowed without an explicit CR and updated gates.

## 9.5 Dependency Update Cadence
inherit from /blueprint/blueprint_v0.1_ch9.md

### 9.5.1 IDE dependency policy (v0.2)
- [VER-15] **Pinned dependency updates**
  - Rules:
    - IDE dependencies added in v0.2 ([BUILD-07]) are pinned and updated only via CR.
    - For security/stability updates:
      - patch-level bumps are preferred when available
      - MINOR/MAJOR bumps require explicit risk assessment (compat + perf + licensing)
  - Consequences:
    - When bumping any of: tree-sitter, PCRE2, SQLite, zstd, xxHash, tomlplusplus, nlohmann/json, spdlog:
      - run the full IDE test suite [TEST-27..34]
      - run perf gates [TEST-35..36]
      - record in decision_delta_log if behavior/ABI assumptions changed.

## 9.6 Release Checklist Requirements
inherit from /blueprint/blueprint_v0.1_ch9.md

### 9.6.1 v0.2 release gates
- [VER-16] A v0.2.0 release tag is allowed only if:
  - all PR CI gates in Ch7 are green
  - nightly tests [TEST-37], [TEST-38] are green within the last 7 days (per [DEC-54])
  - no `[TEMP-DBG]` markers exist ([TEST-01])
  - no `N/A` without `[DEC-XX]` exists ([TEST-02])
  - every new requirement [REQ-63..89] has:
    - ≥1 test mapping (Ch8 “Tooling Test Mapping” and/or Ch3/Ch5)
    - ≥1 checklist step in `/blueprint/implementation_checklist.yaml`

## 9.7 Security & Privacy Lifecycle Notes
inherit from /blueprint/blueprint_v0.1_ch9.md

### 9.7.1 IDE-local data
- [VER-17] Timeline data and IDE session state are treated as user/project-local artifacts:
  - must never be included in cartridge exports
  - must be excluded by default ignore patterns ([DEC-56])
  - must be removable via “Clear IDE Data” command (phase-2 allowed; if deferred, create a follow-up CR)

