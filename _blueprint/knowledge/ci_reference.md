<!-- ci_reference.md -->

# CI Reference (Normative)

This document defines the **minimum CI requirements**, gate catalog, and reference implementations for projects governed by the DET CDD blueprint system.

It is **normative**: CI MUST include the required jobs and MUST enforce the gates described here, unless a higher-precedence rule overrides via DEC + enforcement.

---

## 1. Principles

CI MUST be:
- **Deterministic** (no flaky dependencies without explicit isolation),
- **Cross-platform** (ubuntu/windows/macos matrix),
- **Reproducible locally** (commands documented and runnable),
- **Gate-driven** (fail fast on blueprint violations),
- **Traceable** (gates map to BUILD/TST IDs, CRs, and release docs).

---

## 2. Required CI matrix (XPLAT)

### 2.1 Minimum OS coverage (MUST)

CI MUST run build+test on:
- Ubuntu (latest LTS pinned recommended)
- Windows (latest stable pinned recommended)
- macOS (latest stable pinned recommended)

### 2.2 Compiler coverage (MUST)

At least one compiler variant per OS MUST run. Recommended defaults:
- Ubuntu: GCC and/or Clang (at least one)
- Windows: MSVC (required) + ClangCL optional
- macOS: AppleClang (required)

If an OS lacks a feasible sanitizer/analysis tool (e.g., TSan on Windows), document via DEC and provide a substitute job (static analysis, /analyze).

### 2.3 Architectures (SHOULD)

- x86_64 on all OS
- arm64 on macOS (if runners available) and optionally Linux (if project supports)

---

## 3. Required CI stages (MUST)

Minimum stages (jobs) required:

1. **Blueprint validation** (schema + invariants + Eff(V) composition)
2. **Configure** (CMakePresets.json)
3. **Build**
4. **Unit tests**
5. **Integration tests** (if any; otherwise document absence)
6. **Sanitizer / Analysis job** (ASan+UBSan or equivalent)
7. **Format/Lint** (style enforcement)
8. **Release invariants check** (if overlay exists or on release branches)

CI MUST run these stages per OS where feasible. Some stages (format/lint) MAY run on a single OS if fully deterministic and documented.

---

## 4. Gate catalog (normative)

Gates are grouped by category. Each gate SHOULD correspond to a `BUILD-xxxxx` or `TST-xxxxx` ID in the blueprint.

### 4.1 Blueprint correctness gates

#### GATE: MD header invariant
- **Intent:** enforce `<!-- filename.md -->` first line on all `*.md` under `/_blueprint/`
- **Failure:** missing/malformed header
- **Remediation:** add correct first-line comment matching basename

#### GATE: No forbidden literals
- Forbid:
  - `N/A`
  - `[TEMP-DBG]` (must be absent in final; see temp_dbg_policy)
- Failure includes file + line + match

#### GATE: Composition/inheritance validity (COMP-01)
- Reconstruct Eff(vCURRENT) and (if applicable) Eff(vNEXT)
- Validate:
  - all Ch0..Ch9 exist
  - inherit files are exact and targets resolve
  - rule-chain references resolve or DEC+stub+failing gate exists

#### GATE: ID monotonicity and no renumbering
- For Mode C overlays:
  - ensure IDs are not renumbered vs prior Eff()
  - ensure new IDs advance monotonically per prefix

#### GATE: REQ coverage
- Fail if any REQ lacks:
  - ≥1 TST reference
  - ≥1 checklist step

#### GATE: Release.md invariants
- Version strictly increases vs Release History
- Previous matches the immediately prior patch
- CR list references existing CR files
- Gates list present

### 4.2 Build reproducibility gates

#### GATE: CMakePresets.json required and valid
- Ensure presets exist:
  - per-OS configure/build/test presets
  - `ci-<os>` presets
- Ensure presets use pinned generators/toolchains where specified

#### GATE: Clean configure/build/test
- Configure via presets
- Build via presets
- Run tests via CTest or preset-defined test step

#### GATE: Warnings-as-errors (CI default)
- Ensure CI builds with warnings treated as errors unless DEC allows exceptions.

### 4.3 Testing quality gates

#### GATE: Unit tests required
- At least one unit test suite must run and pass on each OS.

#### GATE: Negative tests (when applicable)
- For security/network/persistence topics, require negative tests and/or fuzzing stubs per policy.

#### GATE: Fuzzing (conditional)
- When fuzzing is required by testing_strategy.md, run fuzzers at least on one OS.
- If fuzzing cannot run in CI, provide DEC + alternate gate (oss-fuzz integration, nightly job, or local-only with failing-fast gating).

### 4.4 Sanitizer / analysis gates

#### GATE: ASan+UBSan job (Linux/macOS)
- Run sanitizer builds with tests.
- If a sanitizer cannot run, DEC + substitute static analysis gate required.

#### GATE: TSan job (where feasible)
- If concurrency-heavy, run TSan at least on Linux or macOS.
- If infeasible, DEC and add an alternate concurrency analysis gate.

#### GATE: Windows analysis
- Prefer:
  - Windows ASan if supported by toolchain, OR
  - MSVC `/analyze` static analysis

### 4.5 Formatting / lint gates

#### GATE: clang-format check
- Verify formatting is compliant (`--dry-run` / `--Werror` behavior).
- Must be deterministic and stable.

#### GATE: clang-tidy (or equivalent)
- Run on at least one OS (Linux recommended).
- Use compile_commands.json from CMake.

### 4.6 Performance gates (conditional)

If the blueprint defines performance budgets (Ch1/Ch5):
- Require:
  - benchmark harness runnable in CI (at least smoke),
  - performance regression checks (relative thresholds),
  - results artifacts saved (optional; depends on CI system).

---

## 5. Reference CI workflow structure (GitHub Actions-style)

This section is **informative** but widely applicable; adapt to your CI system as needed.

### 5.1 Jobs (suggested)

- `blueprint-validate` (linux)
- `format-lint` (linux)
- `build-test-ubuntu` (linux)
- `build-test-windows` (windows)
- `build-test-macos` (macos)
- `sanitizers-asan-ubsan` (linux/macos)
- `sanitizers-tsan` (linux; conditional)
- `static-analysis-windows` (windows)
- `release-invariants` (linux; conditional on release branches/tags)

### 5.2 Required local-equivalent commands

For each job, Ch7 MUST document commands equivalent to CI, typically:

- `cmake --preset ci-linux`
- `cmake --build --preset ci-linux`
- `ctest --preset ci-linux` (or `cmake --build --preset ... --target test`)

Blueprint validator:
- `python3 tools/blueprint/compose_validate.py --current`
or
- `cmake --build --preset ... --target tool_blueprint_validator && <invoke>`

---

## 6. Gate implementation guidance (normative for validator)

The composer/validator script SHOULD implement most blueprint correctness gates to keep CI workflows simple. CI jobs then call the validator.

Validator output MUST:
- print the gate name/ID on failure,
- print file + line where possible,
- include a short remediation hint.

Exit codes:
- 0 success
- non-zero failure (gate violated)

---

## 7. Artifact retention and logs

CI SHOULD store:
- validator report (text or JSON)
- test reports (JUnit XML if supported)
- sanitizer logs
- performance benchmark outputs (if run)

Logs MUST respect redaction policies (see logging_observability_policy_new.md).

---

## 8. Branch and release protections (recommended)

- Require passing CI gates before merge to main.
- Require release invariants gate on tags/releases.
- Enforce CODEOWNERS for `/_blueprint/` and CI configs.

---

## 9. Minimal gate mapping table (recommended)

Projects SHOULD maintain a table in Ch7 or in CI docs mapping gates to jobs.

Example:

| Gate ID | Gate name | Implemented by | Runs on |
|---|---|---|---|
| BUILD-00001 | blueprint-validate | tools/blueprint/compose_validate.py | ubuntu |
| BUILD-00002 | presets-required | validator check | ubuntu |
| TST-00010 | unit-tests | ctest preset | all OS |

---

## 10. CI “must fail” conditions (summary)

CI MUST fail on:
- any `[TEMP-DBG]` occurrences (per policy)
- any `N/A` occurrences
- missing/malformed blueprint MD header comments
- missing Ch0..Ch9 in Eff(V)
- broken inheritance
- REQ without tests or checklist coverage
- invalid or missing `CMakePresets.json`
- missing required OS matrix coverage
- missing sanitizer/analysis job without DEC+substitute
- invalid `release.md` invariants
