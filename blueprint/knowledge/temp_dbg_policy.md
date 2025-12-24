# TEMP-DBG Policy (Normative)
This document defines the strict protocol for **temporary debugging code** to prevent “debug pollution” in production repositories.

> **Rule:** Temporary debug code is allowed only when explicitly marked and must be automatically detected and blocked from merge/release by build/CI gates.

---

## 1) What Counts as TEMP-DBG
TEMP-DBG includes (non-exhaustive):
- `printf` / `std::cout` / `fprintf(stderr, ...)` added for investigation
- hardcoded file paths, magic constants used only for debugging
- ad-hoc timing probes, “quick hacks”, test-only toggles added in production code
- bypasses/short-circuits (e.g., `return` early) used to inspect behavior
- extra logging spam that is not part of the designed logging/telemetry system

If it is not intended to ship, it is TEMP-DBG.

---

## 2) Mandatory Markers (Exact Format)
All TEMP-DBG code MUST be wrapped with these exact markers:

```cpp
// [TEMP-DBG] START <reason> <owner> <date>
...temporary debug code...
// [TEMP-DBG] END
```

### 2.1 Field meanings
- `<reason>`: short reason (e.g., `trace_alloc`, `inspect_latency`, `repro_race`)
- `<owner>`: person/handle responsible for removal
- `<date>`: ISO `YYYY-MM-DD`

### 2.2 Scope rules
- Markers must wrap the **smallest possible** block.
- Do not nest TEMP-DBG blocks.
- Do not place markers in generated files.

---

## 3) What is NOT TEMP-DBG (Allowed Debug Mechanisms)
Prefer these instead of TEMP-DBG:
- the approved logging library (e.g., spdlog/glog) configured with levels
- compile-time debug flags that are part of the design (e.g., `MYLIB_ENABLE_TRACING`)
- runtime config toggles (env/config file) defined in the Blueprint
- sanitizers (ASan/TSan/UBSan), profilers, tracers
- unit tests / repro harnesses under `/tests` or `/tools`

**Rule:** If you can solve it with proper logging or tests, do that first.

---

## 4) Legitimate Logging vs TEMP-DBG
### 4.1 Logging policy
- Use only the approved logger (defined in the Blueprint, e.g., `spdlog`).
- No `std::cout`, no `printf` in production code for logging.
- Logging must be:
  - rate-limited where needed
  - level-based (trace/debug/info/warn/error)
  - structured where useful (key=value fields)

### 4.2 When to add permanent logs
Permanent logs are allowed if:
- they serve a supported operational/debug purpose
- they align with the observability requirements ([REQ-02])
- they have acceptable overhead on hot paths (document if not)

Otherwise, treat as TEMP-DBG.

---

## 5) Enforcement (Mandatory)
The repo MUST include:
- a script in `/tools/` that checks for TEMP-DBG markers in tracked sources
- a CMake target (or equivalent) that runs the script
- a CI gate that fails if any markers exist on merge/release

### 5.1 Recommended check behavior
- Scan tracked files under: `/src`, `/include`, `/tests`, `/tools` (configurable)
- Fail on any match of:
  - `\[TEMP-DBG\] START`
  - `\[TEMP-DBG\] END`
- Print file paths and line numbers for each match
- Return non-zero exit code to fail CI

### 5.2 Example commands (reference)
Developers should be able to run:
- `cmake --build --preset dev --target check_no_temp_dbg`
or directly:
- `python tools/check_no_temp_dbg.py`

---

## 6) TEMP-DBG Lifecycle
### 6.1 Creation rule
When adding TEMP-DBG:
- include markers with reason/owner/date
- keep it minimal and local
- create a follow-up task/CR if investigation will span more than a day

### 6.2 Removal rule
Before merge/release:
- remove all TEMP-DBG blocks
- replace with:
  - permanent logging (if justified), or
  - tests/repro harnesses, or
  - nothing (if no longer needed)

---

## 7) Anti-Patterns (Strictly avoid)
- Leaving TEMP-DBG blocks “just in case”
- Converting TEMP-DBG into permanent `printf` spam
- Hardcoding absolute paths or machine-specific settings
- Debug-only behavior that changes semantics without a feature flag and tests
- Adding debug code to public headers that affects downstream builds

---

## 8) Examples
### 8.1 Good TEMP-DBG
```cpp
// [TEMP-DBG] START inspect_latency bob 2025-12-23
auto t0 = clock::now();
do_work();
auto t1 = clock::now();
std::fprintf(stderr, "dt_ns=%lld\n", (long long)ns(t1 - t0));
// [TEMP-DBG] END
```

### 8.2 Bad (Unmarked)
```cpp
std::cout << "here" << std::endl; // ❌ unmarked, pollutes repo
```

### 8.3 Better (Permanent logging)
```cpp
SPDLOG_DEBUG("phase=do_work dt_ns={}", dt_ns);
```

---

## 9) Quick Checklist
- [ ] Any temporary debug code is wrapped in markers with reason/owner/date
- [ ] No nesting; minimal scope
- [ ] CI/build gate fails if markers remain
- [ ] Prefer proper logging/tests/sanitizers over TEMP-DBG
