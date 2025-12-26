<!-- _blueprint/knowledge/testing_strategy.md -->

# Testing Strategy (Normative)
This document defines a consistent, production-grade **testing strategy** for native C++ repositories (and optional Python bindings).  
It is intended to reduce regressions, improve confidence across versions, and make quality gates enforceable.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Testing Goals
- Catch functional regressions quickly (unit tests).
- Validate real integration behavior (integration tests).
- Detect memory/UB/thread issues (sanitizers + stress).
- Guard performance budgets (bench + thresholds) when required.
- Support lifecycle evolution (compatibility tests + migration verification).

---

## 2) Test Taxonomy (Mandatory definitions)
### 2.1 Unit Tests
- Fast, deterministic, no external dependencies.
- Target: individual functions/classes with controlled inputs.
- Must run on every PR in primary CI.

### 2.2 Integration Tests
- Validate module boundaries, file I/O, IPC/networking if applicable, multi-component behavior.
- May be slower; still should run on PR unless too heavy.

### 2.3 System/End-to-End (Optional)
- Full product execution tests (CLI runs, sample pipelines, demo scenes).
- Typically run on `main` or nightly if expensive.

### 2.4 Concurrency Stress/Soak Tests (As needed)
- Designed to shake races/deadlocks/ordering issues.
- Must run under TSan where possible.

### 2.5 Fuzz Tests (Optional but recommended for parsers/input-heavy code)
- libFuzzer/AFL-style fuzzing harnesses.
- At minimum, provide fuzz “smoke” in CI; full fuzzing can be nightly.

### 2.6 Compatibility Tests (When API/ABI guarantees are claimed)
- Compile/link tests against previous public headers or sample downstream consumer.
- Runtime behavior tests for backward-compat critical behavior.

---

## 3) Test Frameworks (Recommended defaults)
- C++ unit tests: Catch2 or GoogleTest (pick one per project; codify in [BUILD-03])
- Benchmarks: Google Benchmark (if perf budgets matter)
- Python tests: pytest (if Python bindings exist)

**Rule:** Blueprint must declare chosen frameworks and how to run them.

---

## 4) Directory & Naming Conventions
Recommended structure:
```
/tests/
  unit/
  integration/
  stress/
  fuzz/            (optional)
  data/            (golden files, fixtures)
```

Naming:
- `*_test.cpp` for unit tests
- `*_it.cpp` for integration tests (or a consistent alternative)
- `*_stress.cpp` for stress tests
- `fuzz_*` for fuzz targets

**Rule:** Tests must be grouped logically and runnable via CTest labels.

---

## 5) CTest Integration (Mandatory)
- Use CTest as the runner entrypoint.
- Register tests with labels:
  - `unit`, `integration`, `stress`, `fuzz`, `compat`, `python`
- CI can run:
  - quick set on PR: `unit` + key `integration`
  - full set on main/nightly: all labels

**Recommended CI commands:**
- `ctest --preset dev --output-on-failure`
- `ctest --preset asan --output-on-failure`

---

## 6) Determinism & Flake Policy (Mandatory)
- Tests should be deterministic: seeded RNG with fixed seeds.
- Any flaky test must be:
  - quarantined behind a label (e.g., `flaky`) AND
  - tracked via a CR to fix, or removed.

**Rule:** No silent retries in CI as a long-term solution; fix root causes.

---

## 7) Fixtures, Golden Files, and Data
- Store test data under `/tests/data/`.
- Prefer small, human-readable golden files when feasible.
- Golden file updates must be intentional:
  - include a command/script to regenerate
  - review diffs in PR

**Rule:** Do not fetch test data from the network during CI.

---

## 8) Performance & Bench Testing (When budgets exist)
If performance budgets are in [REQ-01], define:
- microbench targets (`<proj>_bench`)
- stable measurement methodology (warmup, iterations)
- regression thresholds that fail CI (or at least warn initially)

If present, CI should run benches on a stable runner type (or nightly).

---

## 9) Sanitizers as Test Multipliers (Mandatory)
- Run tests under ASan and UBSan at minimum on Linux.
- Run stress tests under TSan where supported.
- Ensure the Blueprint defines presets ([BUILD-04]) and how to interpret failures.

**Rule:** A sanitizer failure is a test failure.

---

## 10) Concurrency Testing Guidance
- Add targeted stress tests for:
  - thread pool/task scheduler correctness
  - lock-free structures (if any)
  - data races on shared state
- Provide a minimal reproducer harness and keep it small.

---

## 11) Python Bindings Testing (If applicable)
- Use pytest.
- Include:
  - API surface tests (import, call, exceptions/errors mapping)
  - zero-copy/ownership tests (buffer protocol, numpy views)
  - GIL/threading tests if concurrency involved
- Ensure Python tests can be run via CTest label `python` or via a documented command.

---

## 12) Coverage (Optional policy)
If you want coverage:
- define coverage presets/jobs separately (coverage is expensive)
- prefer line + branch coverage for core modules
- set pragmatic targets (avoid “100% at all costs”)

Blueprint should state whether coverage is required and threshold.

---

## 13) Minimal Acceptance Criteria (Recommended)
For a “healthy” repository:
- Unit tests: fast and always run on PR
- Integration: at least key integration tests run on PR
- Sanitizers: ASan+UBSan on Linux per PR; TSan at least on main/nightly
- No TEMP-DBG markers in repo
- Format gate passes

---

## 14) Compliance Checklist
- [ ] Test taxonomy defined in Blueprint ([TEST-XX] IDs)
- [ ] Tests registered in CTest with labels
- [ ] Unit tests run on every PR
- [ ] ASan+UBSan test runs exist (Linux)
- [ ] Stress/TSan plan exists if concurrency is in scope
- [ ] Python tests exist if bindings exist
- [ ] Bench regression plan exists if perf budgets exist
