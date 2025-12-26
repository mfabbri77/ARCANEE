<!-- performance_benchmark_harness.md -->

# Performance Benchmark Harness (Normative)

This document defines the **benchmarking harness**, measurement rules, and performance regression enforcement for systems governed by the DET CDD blueprint framework.

It is **normative** when:
- performance budgets exist in Ch1/Ch5, or
- profile is `PROFILE_AAA_HFT`, or
- a DEC declares performance as a key constraint.

Otherwise it is recommended guidance.

---

## 1. Goals

- Provide repeatable, comparable performance measurements.
- Detect regressions early via CI gates (where feasible).
- Encode performance budgets as enforceable contracts.
- Support hotpath optimization without sacrificing correctness.

---

## 2. Definitions

- **Benchmark**: a repeatable measurement of an operation’s time/throughput/allocations.
- **Microbenchmark**: small focused benchmark (function-level).
- **Macrobenchmark**: end-to-end scenario benchmark.
- **Budget**: target thresholds for latency/throughput/memory/allocs.
- **Regression**: statistically meaningful performance degradation vs baseline.

---

## 3. Baseline harness requirements (MUST)

### 3.1 Harness location and build integration

Benchmarks MUST live under:
- `/tests/perf/` or `/benchmarks/` (DEC required if separate directory)

Benchmarks MUST:
- be built via CMake,
- produce at least one executable target, e.g. `bench_core`.

### 3.2 Harness framework default

Widely recognized default:
- **Google Benchmark** (`benchmark` library) for microbenchmarks.

Alternative frameworks are allowed via DEC.

### 3.3 Deterministic inputs

Benchmarks MUST:
- use fixed input datasets or generated inputs with fixed seeds,
- avoid nondeterministic IO unless explicitly measured and isolated.

### 3.4 Warmup and iterations

Benchmarks MUST include:
- warmup iterations,
- sufficient repetitions to reduce noise.

Defaults (reasonable baseline):
- warmup: 1–2 seconds or fixed warmup iterations
- measurement: at least 10 iterations (or framework default) with stable seed

---

## 4. Metrics to measure (normative)

At minimum, measure:
- wall-clock time per operation (or per batch)
- throughput (ops/sec) if applicable

If hotpath includes memory constraints:
- measure allocations (count/bytes) where feasible (custom allocator hooks or tooling)
- measure peak RSS in macrobench (optional; noisy)

---

## 5. Environment controls (recommended, sometimes required)

### 5.1 CI performance constraints

CI machines are noisy; therefore:

- CI SHOULD run **smoke benchmarks**:
  - short duration, primarily to catch gross regressions
- Hard gating in CI SHOULD be used only when:
  - CI environment is stable enough, or
  - regression thresholds are generous and measured robustly

If CI cannot reliably gate performance:
- a DEC MUST define an alternate enforcement:
  - nightly performance runs
  - dedicated perf runner
  - trend reporting only + manual approval gate

### 5.2 Recommended controls

- pin CPU frequency governor if possible (dedicated runners)
- pin process affinity (optional)
- isolate background work (best-effort)

---

## 6. Regression policy (normative when budgets exist)

### 6.1 Threshold definition

Ch1/Ch5 MUST define:
- target metrics (p50/p95 latency, throughput, allocations)
- acceptable regression thresholds per metric
- what constitutes “budget violation”

Default rational thresholds (if none provided, choose via DEC):
- microbench: fail if >10% regression on median time
- macrobench: warn at 10%, fail at 20%
- allocations: fail if allocations increase on hotpath unless DEC

### 6.2 Baseline selection

Regression checks MUST compare against:
- last release baseline (preferred), or
- pinned baseline commit, or
- stored CI artifact baseline

Baselines MUST be explicit and immutable.

---

## 7. Benchmark output format (normative)

Benchmarks MUST be able to emit machine-readable output:
- JSON preferred (Google Benchmark supports `--benchmark_format=json`)

CI SHOULD archive:
- benchmark JSON outputs
- environment summary (compiler version, CPU, OS)

---

## 8. Benchmark categories and required coverage

If performance is a key constraint, ensure benchmarks exist for:
- hotpath critical operations (submit/route/match equivalent)
- serialization/deserialization (if applicable)
- concurrency queues/backpressure operations (if applicable)
- memory allocation hotspots (if applicable)

---

## 9. CI integration (normative guidance)

### 9.1 Required jobs (conditional)

If budgets exist, CI MUST include at least:
- `bench-smoke` job (Linux recommended)

### 9.2 Example commands

- Configure/build via presets:
  - `cmake --preset ci-linux`
  - `cmake --build --preset ci-linux --target bench_core`
- Run:
  - `./build/ci-linux/bench_core --benchmark_format=json --benchmark_out=bench.json`

(Exact paths depend on presets; Ch7 must document.)

### 9.3 Regression gate implementation

A regression gate script SHOULD:
- parse benchmark JSON,
- compare to baseline JSON,
- compute percent change,
- fail if thresholds exceeded,
- print the top regressions.

---

## 10. Determinism and floating-point considerations (AAA/HFT)

If determinism profile:
- benchmark harness MUST:
  - fix seeds and time sources
  - document FP flags (fast-math vs strict)
  - avoid logging overhead on hotpath

If FP controls are required:
- add gates ensuring consistent compiler flags per platform.

---

## 11. Testing and validation of benchmarks

Benchmarks SHOULD include:
- correctness assertions where feasible (e.g., output invariants)
- guards against compiler optimizing away work (use `DoNotOptimize`, etc.)

---

## 12. Enforcement summary

When performance budgets exist, CI/validator MUST ensure:
- benchmarks build successfully
- smoke run executes (non-zero output)
- regression checks run via defined policy (hard gate or alternate per DEC)
- benchmark outputs are archived or printed

---

## 13. Policy changes

Changes to this harness MUST be introduced via CR and MUST include:
- updated scripts/gates for regression checks,
- migration notes for benchmark naming/output.
