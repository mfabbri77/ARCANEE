# Performance Benchmark Harness (Normative)
This document standardizes how to create and run **performance benchmarks** for native C++ projects (and optional Python bindings) and how to enforce **regression gates** over time.

> Use this when the Blueprint defines performance budgets ([REQ-01]) or regression policies ([VER-09]).  
> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Goals
- Make performance measurable, repeatable, and comparable across versions.
- Detect regressions early and tie them to budgets (latency/throughput/memory).
- Avoid misleading benchmark results (noise, warmup issues, unstable environments).
- Provide a clean path for CI gating and long-term baselines.

---

## 2) Benchmark Types (Recommended)
### 2.1 Microbenchmarks (primary)
- Measure single operations or tight kernels (allocators, queue ops, math kernels, encoding).
- Use stable inputs and fixed-size datasets.

### 2.2 Scenario/Workload benchmarks (optional)
- Measure realistic pipelines (frame simulation, asset upload, command buffer recording).
- Often more representative but also noisier.

### 2.3 End-to-end latency sampling (optional)
- If you care about p50/p99/p999 latency, add a harness that records a latency distribution.

---

## 3) Framework Choice (Recommended default)
- **Google Benchmark** for C++ microbenchmarks.

**Rule:** The Blueprint must specify the benchmark framework and how to build/run it.

---

## 4) Repository Structure & Targets
Recommended:
```
/bench/
  benchmarks/         # *.cpp google-benchmark sources
  data/               # optional datasets
```

Targets:
- `<proj>_bench` (single binary) or multiple per module.

**Rule:** Bench targets must be built via CMake presets and runnable without extra manual steps.

---

## 5) Measurement Discipline (Mandatory rules)
### 5.1 Warmup
- Always include warmup iterations/time to reduce cold-start noise (caches, JIT-like effects, drivers).
- For GPU-related benches, ensure first-run initialization is excluded or explicitly measured separately.

### 5.2 Stable inputs
- Avoid random inputs unless seeded and controlled.
- Prefer fixed datasets; document size and distribution.

### 5.3 Avoid I/O in measurement
- No disk/network I/O inside measured loops.
- Pre-load data outside timed region.

### 5.4 CPU affinity and turbo (recommended)
- If possible for dedicated benchmarking machines:
  - pin CPU affinity
  - disable CPU frequency scaling / turbo for stable results
- In CI, assume noisy environment and use conservative gates.

---

## 6) What to Measure (Recommended metrics)
Depending on project needs, record:
- **Throughput** (ops/s, MB/s, frames/s)
- **Latency** (ns/op) for microbench
- **Distribution** (p50/p99/p999) for scenario benches
- **Memory** (peak allocations, bytes allocated/op) where feasible
- **GPU timing** (optional) using timestamp queries, if relevant and stable

**Rule:** Every benchmark must declare what metric matters and what budget it maps to ([REQ-01]).

---

## 7) Output Format & Baselines
### 7.1 JSON output
Google Benchmark supports JSON output. Standardize:
- `--benchmark_format=json`
- store results as artifacts in CI for comparison.

### 7.2 Baselines storage
Recommended baseline approach:
- commit a baseline JSON under `/bench/baselines/vX.Y.json` for major releases
- or store baselines in CI artifacts / a dedicated performance dashboard

**Rule:** Baselines must be versioned and tied to releases (SemVer).

---

## 8) Regression Policy (Mandatory if perf budgets exist)
Blueprint should define [VER-09] including:
- which benchmarks are gating
- regression thresholds
- how noise is handled

Recommended threshold patterns:
- **Hard fail** if regression > X% on a stable runner
- **Warn-only** on PR CI (noisy) and enforce on nightly/stable perf runners
- Use different thresholds per benchmark category

Example policy:
- Microbench: fail if > 10% regression
- Scenario: fail if > 15% regression
- Memory: fail if > 10% increase in peak allocations

**Rule:** Thresholds must be explicit and documented in the Blueprint.

---

## 9) CI Integration (Recommended)
### 9.1 Two-tier model (recommended)
- **PR CI:** run a small subset, record results, optionally warn on large regressions
- **Nightly/Perf CI:** run full suite on dedicated runner and enforce strict thresholds

### 9.2 Commands (examples)
- Build: `cmake --preset release && cmake --build --preset release --target <proj>_bench`
- Run: `./build/release/<proj>_bench --benchmark_format=json --benchmark_out=bench.json`

**Rule:** Keep benchmark execution separate from unit tests; do not slow down PR CI unnecessarily.

---

## 10) GPU Benchmarks (If applicable)
GPU timing is tricky; if you add it:
- use timestamp queries or platform timing tools
- separate “submission cost” (CPU) from “GPU execution time”
- warm up pipelines and resources before timing
- avoid measuring swapchain/present timing unless explicitly needed

**Rule:** GPU benches must document what exactly they measure (CPU submission vs GPU time).

---

## 11) Python Benchmarks (If Python bindings exist)
- Prefer `pytest-benchmark` or a small dedicated harness.
- Measure:
  - overhead of binding calls
  - zero-copy view creation cost
  - batch operation performance

**Rule:** Python benchmarks must avoid per-element Python loops where possible; measure meaningful, vectorized usage.

---

## 12) Anti-Patterns (Avoid)
- Comparing results across different machines without noting hardware differences.
- Running heavy background tasks during measurement.
- Measuring one-off first-run initialization as “steady state” accidentally.
- Treating noisy CI results as definitive without controlling environment.
- Gating on extremely tight thresholds without stable runners.

---

## 13) Quick Checklist
- [ ] Bench target exists and builds via presets
- [ ] Warmup is handled
- [ ] Outputs JSON and stores artifacts/baselines
- [ ] Regression thresholds are defined ([VER-09]) and enforceable
- [ ] CI has a plan (PR subset + nightly/stable runner recommended)
- [ ] GPU and Python benchmarks are explicit and scoped (if applicable)
