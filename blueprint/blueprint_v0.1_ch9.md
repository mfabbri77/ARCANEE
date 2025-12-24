# Blueprint v0.1 — Chapter 9: Versioning, Lifecycle, Governance

## 9.1 SemVer Policy
- [VER-01] Use SemVer: `MAJOR.MINOR.PATCH`.
- [VER-02] v1.0.0 is the first stable release (runtime + workbench).
- [VER-03] PATCH: bug fixes + tests; no contract changes.
- [VER-04] MINOR: additive feature changes; may include deprecations with migrations.
- [VER-05] MAJOR: breaking changes to script API, file formats, or published contracts.

## 9.2 Deprecation & Migration
- [VER-06] Deprecations must:
  - be documented in `/blueprint/` and release notes
  - have a removal version
  - include a migration path
- [REQ-60] Provide `MIGRATION.md` for any MINOR+ changes that affect cartridges.

## 9.3 CR Governance
- [VER-07] All blueprint-impacting work requires a CR:
  - `/blueprint/cr/CR-XXXX.md` describing delta, risk, tests, migration notes.
- [VER-08] CRs must include perf/regression considerations and CI gates affected.

## 9.4 Dependency Cadence & Security
- [REQ-61] Dependencies are pinned; updates occur on a scheduled cadence (e.g., monthly/quarterly) or for security fixes.
- [REQ-62] Maintain a third-party license notice bundle in packaging output.

## 9.5 Performance Regression Policy
- [VER-09] Define “perf budgets” as tracked metrics:
  - tick time (p50/p95)
  - frame time stability
  - allocation counts in hot paths
- [TEST-26] `perf_smoke_baseline`: records metrics and fails if regression exceeds threshold (thresholds set per platform).


