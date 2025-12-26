<!-- temp_dbg_policy.md -->

# Temporary Debug Policy (Normative)

This policy governs the creation and removal of **temporary debug code** in repositories governed by the DET CDD blueprint system.

It is **normative** and is enforced by CI gates:
- any temporary debug left behind MUST fail CI.

---

## 1. Purpose

Temporary debug code is a common source of:
- performance regressions,
- accidental behavior changes,
- information leakage,
- non-determinism,
- noise in logs/tests.

This policy allows temporary debug as a controlled exception, while ensuring it cannot ship.

---

## 2. Definitions

- **TEMP debug**: any code, logging, instrumentation, hacks, or bypasses introduced solely for short-term debugging or experimentation.
- **Marker**: the literal string `[TEMP-DBG]` used to denote temporary debug.
- **Scope**: files where TEMP debug may exist (see §4).

---

## 3. Core rules (MUST)

### 3.1 Marking requirement

Any TEMP debug MUST be marked with the literal marker:

- `[TEMP-DBG]`

Rules:
- Marker MUST appear in the same line as:
  - the temp code, or
  - the nearest comment immediately above it.
- Marker MUST be unmodified:
  - exact capitalization,
  - exact brackets.

### 3.2 Prohibition in mainline

TEMP debug MUST NOT exist in:
- releases,
- mainline protected branches (as defined by CI),
- any PR/merge that passes required gates.

CI MUST fail if any `[TEMP-DBG]` markers remain anywhere in the repo (or in the configured scan scope).

### 3.3 No unmarked TEMP debug

Unmarked temporary debug is forbidden.

If a developer adds TEMP debug, they MUST mark it. CI MAY optionally include heuristic scans (e.g., “TODO remove”, “HACK”) but the canonical rule is the marker.

### 3.4 Expiry requirement (SHOULD)

When TEMP debug is used, it SHOULD include:
- an issue/ticket reference, or
- a short “remove by” note,

on the same line or adjacent.

Example:
```cpp
// [TEMP-DBG] ticket:ABC-123 remove after fixing race in router
```

---

## 4. Scan scope (normative default)

### 4.1 Default scan scope

CI MUST scan at minimum:
- `/src`
- `/include`
- `/tests`
- `/tools`

CI MAY scan additional directories (recommended).

### 4.2 Exclusions (allowed by DEC only)

Excluding directories from scanning is only allowed via DEC + gate that:
- documents why exclusion is safe,
- ensures excluded paths cannot ship (e.g., not packaged),
- ensures excluded paths are not executed in CI.

---

## 5. Enforcement gates (MUST)

### 5.1 Gate: temp debug marker scan

CI MUST include a gate that:
- recursively scans the scan scope for `[TEMP-DBG]`,
- fails if any matches exist,
- prints:
  - file path,
  - line number,
  - a few surrounding characters for context.

Recommended implementations:
- Python script for cross-platform behavior, OR
- ripgrep with platform-safe invocation.

### 5.2 Gate: no unmarked debug (optional, recommended)

CI SHOULD include a heuristic gate that detects likely TEMP debug patterns, such as:
- `printf(` / `std::cout` added in non-test code,
- `sleep(`,
- `#if 0` blocks,
- comments like “REMOVE”, “HACK”, “DEBUG ONLY”,

and fails unless the code is properly marked `[TEMP-DBG]`.

This gate MUST have low false positives; if too noisy, it should be disabled or narrowed via DEC.

---

## 6. Developer workflow (recommended)

### 6.1 How to use TEMP debug safely

1. Add TEMP debug with `[TEMP-DBG]` marker.
2. Add or update a test that reproduces the bug (preferred).
3. Use TEMP debug to diagnose.
4. Remove TEMP debug.
5. Ensure CI passes.

### 6.2 Local preflight

Provide a local command (documented in Ch7) to run the scan:
- `python3 tools/ci/scan_temp_dbg.py` (example)

---

## 7. Interactions with logging policy

TEMP debug MUST NOT be used as a substitute for:
- proper structured logging per logging_observability_policy_new.md
- instrumentation via metrics/traces

Use TEMP debug only for short-lived investigations.

---

## 8. Policy changes

Changes to this policy MUST be introduced via CR and MUST include:
- updated enforcement scripts/gates,
- updates to CI reference docs,
- migration steps if scan scope changes.
