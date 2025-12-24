# Logging & Observability Policy (Normative)
This document standardizes **logging**, optional **metrics**, and optional **tracing** for native C++ repositories (and optional tooling).  
It is designed to replace ad-hoc debug prints, support long-term maintenance, and keep overhead under control.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Goals
- Provide actionable diagnostics in production and development.
- Prevent “debug pollution” by making proper observability the default.
- Keep hot-path overhead bounded and measurable.
- Ensure logs are consistent across modules and versions.

---

## 2) Choose an Observability Stack (Mandatory Blueprint decision)
Blueprint must choose and record under **[REQ-02]** + **[DEC-XX]**:
- **Logging library:** `spdlog` (recommended) or `glog`
- **Metrics:** optional (e.g., Prometheus exposition, custom counters)
- **Tracing:** optional (e.g., Chrome trace JSON, ETW on Windows, os_signpost on macOS)

**Rule:** Only one primary logging library per repo.

---

## 3) Logging Rules (Mandatory)
### 3.1 No printf/cout
- `std::cout`, `printf`, `fprintf` are forbidden for operational logging.
- Temporary prints are allowed only under `[TEMP-DBG]` markers (see `temp_dbg_policy.md`).

### 3.2 Log levels
Standardize levels:
- `trace`, `debug`, `info`, `warn`, `error`, `critical`

Rules:
- `trace/debug`: dev-only by default; must be cheap when disabled
- `info`: important lifecycle events (startup/shutdown/backend selection)
- `warn/error`: unexpected states and recoverable failures
- `critical`: unrecoverable errors

### 3.3 Structured fields
Prefer structured logging style:
- `event=<name> key=value ...`

Recommended baseline fields:
- `event`
- `subsystem` (core/vulkan/metal/dx12/tools)
- `code` (canonical ErrorCode if relevant)
- `backend_subcode` (VkResult/HRESULT/etc where relevant)

### 3.4 Rate limiting
For logs that may spam (per-frame/per-tick errors, repeated failures):
- implement rate limiting or “log once” helpers.
- do not spam hot paths.

**Rule:** If a log can happen per frame/tick/request, it must be rate-limited or guarded.

---

## 4) Performance & Hot-Path Policy (Mandatory)
- Logging in hot paths must be:
  - behind log level checks
  - minimized allocations (avoid formatting when disabled)
  - optionally compiled out for release (`SPDLOG_ACTIVE_LEVEL` etc.)

**Rule:** The Blueprint must state whether trace/debug logs are compiled out in release and how.

---

## 5) Error Logging vs Error Returning (Mandatory)
- Libraries should primarily **return errors**; applications decide when to log.
- Avoid double-logging:
  - if a function returns a `Status/Error`, it should not also log by default unless it is a boundary layer or a critical diagnostic point.

**Rule:** Define where logging occurs (boundary policy) in the Blueprint.

---

## 6) Metrics (Optional but recommended for long-running tools/services)
If metrics are enabled, define:
- counters (events)
- gauges (current states)
- histograms (latency distributions) if feasible

Recommended baseline metrics:
- init time
- frame/tick time (if applicable)
- memory usage (allocator stats)
- backend selection/device info (as labels or logs)

**Rule:** Metrics collection must have bounded overhead.

---

## 7) Tracing (Optional)
If tracing is enabled:
- define a minimal API:
  - `TRACE_BEGIN(name)` / `TRACE_END()`
  - or scoped RAII `TraceScope`
- choose backend per platform:
  - Windows: ETW (advanced) or JSON chrome trace
  - macOS: os_signpost (advanced) or JSON
  - Linux: perfetto/JSON

**Rule:** Tracing must not be required for correctness; it must degrade gracefully.

---

## 8) Tooling Observability (If tools exist)
- Tools should log to stderr by default.
- Provide `--log-level` or env var override if tools are user-facing.
- Avoid verbose logs unless requested.

---

## 9) Configuration (Mandatory)
Define how observability is configured:
- compile-time defaults (dev vs release)
- runtime overrides:
  - env vars
  - config file
  - CLI flags (apps/tools)

Recommended:
- `LOG_LEVEL`
- `LOG_SINK` (stderr/file)
- `LOG_FORMAT` (text/json)

**Rule:** No hardcoded paths; log paths must be configurable.

---

## 10) Testing (Recommended)
- Unit tests for:
  - log level changes
  - rate limiting utilities
- Integration smoke: run a minimal workload and ensure no crash and reasonable logs.

**Rule:** Tests should not assert exact log strings unless necessary; assert presence of events/levels when needed.

---

## 11) Anti-Patterns (Avoid)
- Logging inside tight loops without guards.
- Logging huge payloads (buffers, shader code) by default.
- Using logs as the only error contract (errors must be returned).
- Platform-specific logging bypassing the configured logger without justification.

---

## 12) Quick Checklist
- [ ] Single logger chosen (spdlog/glog)
- [ ] No printf/cout usage outside TEMP-DBG
- [ ] Levels and structured fields standardized
- [ ] Rate limiting/log-once exists for spammy paths
- [ ] Hot-path overhead policy defined and enforced
- [ ] Configurable sinks/levels with no hardcoded paths
