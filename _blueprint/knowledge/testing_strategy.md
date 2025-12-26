<!-- testing_strategy.md -->

# Testing Strategy (Normative)

This document defines the **minimum testing requirements**, recommended test taxonomy, and enforcement expectations for systems governed by the DET CDD blueprint framework.

It is **normative** unless overridden via DEC + enforcement.

---

## 1. Goals

- Make requirements verifiable: every **REQ** must have tests.
- Prevent regressions via CI.
- Ensure cross-platform correctness.
- Provide confidence in determinism and performance constraints where applicable.
- Provide a structured test pyramid and clear ownership.

---

## 2. Normative core rules

### 2.1 Traceability (MUST)

- Every requirement `REQ-xxxxx` MUST map to:
  - ≥1 test `TST-xxxxx`, and
  - ≥1 checklist step (see `implementation_checklist_schema.md`).
- Every accepted decision `DEC-xxxxx` MUST be enforced by:
  - tests and/or CI gates (prefer both when feasible).

### 2.2 Determinism (conditional MUST)

If the active profile or requirements include determinism constraints:
- tests MUST verify deterministic behavior under controlled inputs:
  - stable random seed
  - injected time source
  - fixed scheduling assumptions where relevant
- CI SHOULD include a “repeatability” gate: run key tests multiple times and compare outputs.

### 2.3 Negative testing (conditional MUST)

If TOPIC includes any of:
- `security`, `network`, `persistence`

Then the project MUST include:
- negative tests (invalid inputs, malformed packets/records),
- fuzzing strategy or stub gate (see §6),
- dependency audit/scan gate (per dependency policy),
- redaction tests for logging (per logging policy).

### 2.4 No untested contracts

Subsystem contracts (API/lifetime/threading/errors/perf) MUST be validated by tests:
- unit tests for core invariants
- integration tests for subsystem interactions
- stress tests for concurrency/backpressure if applicable

---

## 3. Test taxonomy

### 3.1 Test types (supported)

- **Unit**: single class/function/module, no external IO.
- **Integration**: multiple components; may use local files, in-memory DB, loopback sockets.
- **Property-based**: randomized generation, invariant checking (seeded for determinism).
- **Negative**: explicit invalid/failure cases.
- **Stress**: concurrency, load, long-run, resource pressure.
- **Fuzz**: automated adversarial input exploration.
- **Performance**: benchmarks and regression checks.

### 3.2 Recommended pyramid (default)

- Many unit tests
- Some integration tests
- Few end-to-end tests (if applicable)
- Continuous fuzz/perf where risk warrants

---

## 4. Framework and tooling defaults

### 4.1 C++ test frameworks (default)

Widely recognized defaults:
- **GoogleTest** for unit/integration.
- Catch2 acceptable via DEC.

### 4.2 Property testing (optional)

If property tests are needed, options include:
- RapidCheck
- Hypothesis via Python wrappers (if binding layer exists)

Choice requires DEC and dependency approval.

### 4.3 Coverage (recommended)

- CI SHOULD collect coverage reports on at least one platform (Linux recommended).
- Coverage goals are informational by default (avoid hard thresholds early unless needed).

---

## 5. Test structure and naming

### 5.1 Directory layout (normative default)

- `/tests/unit/<module>/test_<thing>.cpp`
- `/tests/integration/test_<scenario>.cpp`
- `/tests/negative/test_<failure>.cpp`
- `/tests/stress/test_<load>.cpp`
- `/tests/fuzz/fuzz_<target>.cpp` (if used)
- `/tests/perf/bench_<thing>.cpp` (or `/benchmarks` via DEC)

### 5.2 Naming conventions

- Test executables: `test_<suite>` (CMake target prefix required)
- Individual test names should be descriptive and stable.
- Avoid embedding random values in names or outputs.

---

## 6. Fuzzing strategy (conditional)

### 6.1 When fuzzing is required

Fuzzing MUST be considered when:
- parsing untrusted input (network, files, messages),
- security-sensitive surfaces exist,
- complex state machines exist.

### 6.2 Minimal compliance levels

One of the following MUST be implemented (choose via DEC if not obvious):

- **Level 1 (stub gate)**:
  - Provide fuzz targets in `/tests/fuzz/`
  - Provide a CI gate that ensures they compile and can run for a small fixed time locally (may be nightly)
- **Level 2 (CI smoke fuzz)**:
  - Run fuzz targets for a short duration in CI on Linux
- **Level 3 (continuous fuzz)**:
  - Integrate with OSS-Fuzz or an internal fuzzing service
  - Nightly/continuous runs with corpus retention

If fuzzing cannot run in CI due to environment constraints, a failing-fast gate MUST require explicit operator run or scheduled job.

---

## 7. Concurrency and stress testing (conditional)

If concurrency is present (Ch6 indicates multi-threading, queues, lock-free structures):
- MUST have at least:
  - one stress test for contention/backpressure,
  - one negative test for shutdown/cancellation behavior,
  - TSan job where feasible (or DEC alternative).

---

## 8. Performance testing (conditional)

If performance budgets are declared (Ch1/Ch5):
- MUST include a benchmark harness (see `performance_benchmark_harness.md`).
- CI SHOULD:
  - run microbenchmarks in smoke mode (short)
  - enforce regression thresholds for key metrics (DEC defines thresholds)
- Benchmarks MUST be repeatable:
  - pin CPU affinity if possible (informational)
  - isolate noise (warmup iterations)
  - stable input datasets

If CI cannot provide stable performance measurements, use:
- trend-only reporting in CI + hard gating in dedicated perf environment (DEC required).

---

## 9. CI integration (normative)

### 9.1 Required CI execution

CI MUST run:
- unit tests on all OS
- integration tests where applicable (at least on one OS; better on all)
- sanitizer/analysis job per `ci_reference.md`

### 9.2 Deterministic test commands

Ch7 MUST specify exact commands to run tests locally that match CI.

---

## 10. Test artifacts and reporting (recommended)

- Emit JUnit XML if possible.
- Store sanitizer logs as artifacts.
- For fuzzing/perf, store seeds/corpora/results.

---

## 11. Common “must-have” test patterns (recommended)

- API contract tests:
  - ownership/lifetime correctness
  - threading usage correctness (where enforceable)
  - error handling and error codes mapping
- Serialization/deserialization round-trip tests
- Resource cleanup and shutdown tests
- Boundary condition tests:
  - empty inputs
  - maximum sizes
  - malformed inputs

---

## 12. Enforcement gates (summary)

CI/validator MUST fail if:
- any REQ lacks TST coverage
- any REQ lacks checklist coverage
- required negative/fuzz/security tests are missing when topic requires them (or missing DEC + stub gates)
- tests are not runnable via presets/ctest
- sanitizer/analysis jobs missing without DEC substitute

---

## 13. Policy changes

Changes to this testing strategy MUST be introduced via CR and MUST include:
- updated gates/tests expectations,
- migration guidance for existing projects.
