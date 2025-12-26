<!-- _blueprint/project/ch9.md -->

# Blueprint v0.3 — Chapter 9: Versioning, Lifecycle, Governance

## Versioning Baseline
inherit from blueprint_v0.2_ch9.md

---

## [VER-18] Release SemVer Target (v0.3)
- Ship this change set as **v0.3.0** (MINOR bump from v0.2.x).
- Rationale: introduces new user-visible configuration surface (`./config`), new theming model, and new highlighting/keybinding configuration features. [CR-0044]

---

## [VER-19] Configuration Compatibility Policy (v0.3)
### Config files are user data
- `config/*.toml` are user-modifiable and must remain forward-compatible where practical.

### Allowed changes
- Adding new keys: allowed (minor/patch), defaults apply if missing.
- Deprecating keys: allowed (minor), must:
  - keep old key supported for ≥1 minor release
  - emit a warning diagnostic when old key is used
  - document replacement key in release notes (Walkthrough + Checklist).

### Breaking changes
- Removing or changing meaning of existing keys: requires MINOR bump + explicit migration notes and tests.

### Unknown keys policy
- Unknown keys produce warnings and are ignored. [REQ-97]
- The IDE does not round-trip config TOML in v0.3; therefore preserving unknown keys is not required for file writing (only for reading tolerance).

---

## [DEC-62] Legacy Settings Model Deprecation
- The v0.2 blueprint described layered settings/keybindings in user data + `.arcanee/` project folders.
- v0.3 **replaces** that model with repo-root `./config/*.toml`. [REQ-90]
- If future work reintroduces project-specific config layers, it must be a dedicated CR with a precedence policy and explicit migration/compatibility tests.

---

## [VER-20] Theme/Token Taxonomy Stability
- The IDE-owned token set used by schemes is a compatibility surface:
  - `comment`, `string`, `number`, `keyword`, `type`, `function`, `variable`, `operator`, `error` (v0.3 stable).
- Adding tokens is allowed (MINOR), but must:
  - provide defaults (fallback color mapping)
  - update `config/color-schemes.toml` template generation and built-ins
  - include tests ensuring old schemes still render (fallback to foreground if missing). [REQ-91], [TEST-40]

---

## [VER-21] Keybinding Action Registry Stability
- Action IDs are stable strings (e.g., `file.save`, `editor.copy`). [REQ-96]
- Changing/removing action IDs requires:
  - MINOR bump
  - deprecation shim (old id remains recognized) for ≥1 minor
  - warning diagnostic if old id is used in `config/keys.toml`
  - documentation + migration note in Walkthrough/Checklist.

---

## [VER-22] Performance Regression Policy for Hot-Apply
- Hot-apply must not introduce frame hitches beyond the “config save” moment:
  - Theme apply: should be negligible (<1ms typical).
  - Font rebuild may be heavier, but must occur only on config save and must be debounced. [REQ-91]
- Add perf guardrails in CI where available:
  - a micro-benchmark for theme apply + keymap apply
  - a smoke benchmark measuring max frame time during a hot-apply event (optional).

---

## [VER-23] CR Governance
- All future changes to:
  - config schema/keys
  - token taxonomy
  - action registry
  - config file locations / precedence
require a dedicated CR referencing this chapter’s compatibility policies. [VER-19..21]

---

## [VER-24] Security / Trust Boundaries (Config)
- Config files are local trusted inputs; nevertheless:
  - Do not allow file path loading for fonts. [REQ-92]
  - Validate numeric ranges (sizes, metrics) to prevent resource exhaustion or crashes. [REQ-91]
  - Defensive parsing with bounded recursion/allocations; parse off-thread; apply on main thread with safe swap. [ARCH-12]

---

## [TEST] Versioning/Compatibility Gates
- [TEST-39] includes coverage that unknown keys warn and do not crash.
- [TEST-40] includes coverage that schemes missing new tokens fallback safely.
- [TEST-41] includes coverage that unknown actions warn and are ignored (binding dropped) without crashing.

