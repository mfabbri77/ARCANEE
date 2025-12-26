<!-- logging_observability_policy_new.md -->

# Logging & Observability Policy (New) (Normative)

This policy defines the **logging, metrics, and tracing** standards for systems governed by the DET CDD blueprint framework.

It is designed for **deterministic, performance-sensitive** systems (including AAA/HFT) and emphasizes:
- explicit budgets,
- redaction and privacy,
- stable schemas,
- testable enforcement.

This file is **normative** unless a higher-precedence rule overrides it via DEC + gates.

---

## 1. Goals

- Provide actionable observability without violating performance budgets or determinism.
- Prevent sensitive data leakage (PII/secrets).
- Ensure logs/metrics/traces are structured, consistent, and machine-parseable.
- Ensure observability behavior is testable and enforceable by CI gates.

---

## 2. Normative principles

### 2.1 Structured first

- Logs MUST be structured (key/value) rather than unstructured prose.
- The system MUST define a stable event schema (see §4).
- Human-readable formatting MAY be generated as a view over structured events.

### 2.2 Determinism-aware

In determinism-sensitive profiles (AAA/HFT):
- Logging MUST be bounded by strict overhead budgets.
- Logging on hotpath MUST be either:
  - disabled, or
  - compiled out, or
  - routed to a lock-free bounded buffer with loss strategy (DEC required).
- Logging MUST NOT introduce nondeterministic ordering that affects program behavior.
- Time source usage MUST follow determinism policy (see §6).

### 2.3 Redaction and privacy

- Sensitive fields MUST be redacted or hashed before emission.
- Secrets MUST NEVER be logged (tokens, private keys, credentials).
- PII MUST be prohibited unless explicitly allowed via DEC and data governance.

### 2.4 Observability is part of contracts

For each non-trivial subsystem contract (see system prompt), the contract MUST specify:
- log events emitted (names, fields),
- metrics emitted (names, units),
- traces/spans emitted (names),
- sampling and rate limits,
- redaction strategy,
- enforcement tests/gates.

---

## 3. Definitions

- **Log event**: structured record with name, severity, timestamp (or logical time), and fields.
- **Metric**: numeric measurement with name, unit, and labels/tags.
- **Trace/span**: correlated timing context for request/operation flows.
- **Hotpath**: code path with strict latency constraints (defined in Ch5).
- **Budget**: explicit maximum overhead for observability (time/memory/cpu).

---

## 4. Logging schema (normative)

### 4.1 Event fields (required)

Every log event MUST include:
- `event` (string): stable event name (snake_case)
- `severity` (enum): `trace|debug|info|warn|error|fatal`
- `component` (string): component identifier (matches Ch3 component name)
- `msg` (string): short human-readable summary
- `ts` (string or integer):
  - general profile: wall-clock timestamp in ISO 8601 or epoch ns
  - determinism profile: **logical time** or monotonic tick (see §6)
- `fields` (object/map): additional structured fields

Optional but recommended:
- `thread` (string/int)
- `correlation_id` (string) for request-scoped tracing
- `build_id` / `version`

### 4.2 Event naming

- Names MUST be stable and versioned only when breaking schema changes occur.
- Use pattern: `<domain>_<action>_<result>` (e.g., `order_submit_ok`, `cache_miss`).

### 4.3 Severity rules

- `error` and above MUST be reserved for actionable faults.
- `debug` MUST be disabled by default in production builds unless a DEC says otherwise.
- `trace` MUST be compile-time disabled by default in performance-sensitive profiles.

### 4.4 Forbidden fields

Logs MUST NOT include:
- secrets: `password`, `token`, `secret`, `private_key`, etc.
- raw payload dumps of user content unless explicitly allowed via DEC + redaction.
- unbounded arrays/strings (must be truncated or summarized).

---

## 5. Metrics policy (normative)

### 5.1 Metric types and units

Metrics MUST use standard types:
- counters
- gauges
- histograms (preferred for latency)

Units MUST be explicit in metric name or metadata:
- `_seconds`, `_milliseconds`, `_bytes`, `_count`

### 5.2 Label cardinality

Labels/tags MUST be bounded. High-cardinality labels (user_id, order_id) are forbidden unless DEC + gate.

### 5.3 Required metrics (baseline)

Projects SHOULD include at minimum:
- uptime / build version
- request/operation counts
- error counts
- latency histogram for key operations
- queue depth/backpressure indicators (if concurrency uses queues)

---

## 6. Time source and determinism (normative)

### 6.1 General profile

- Logs MAY use wall clock time from `std::chrono::system_clock`.
- For ordering, use monotonic time for durations (`steady_clock`).

### 6.2 Deterministic profiles (AAA/HFT)

- Logs MUST NOT use wall clock to influence behavior.
- Logging timestamps MUST use:
  - a deterministic logical tick, or
  - a monotonic tick captured at boundaries that do not affect determinism.

A DEC MUST specify the time source strategy, including:
- which clocks are used and where,
- whether time is injected (dependency inversion) for testing,
- how drift is handled in production.

---

## 7. Sampling and rate limiting (normative)

- Hotpath log emission MUST be rate-limited or sampled.
- The system MUST define defaults:
  - max events/sec per component
  - burst limits
  - sampling percentages per severity

If tracing is enabled:
- tracing MUST be sampled by default to avoid overhead.

---

## 8. Error reporting and correlation (normative)

When an error is logged:
- include a stable error code (see error handling playbook)
- include correlation_id if available
- include root-cause hints (not stack dumps by default in production)

---

## 9. Storage, transport, and sinks (normative guidance)

Projects MUST define at least one log sink:
- stdout/stderr for CLI tools
- file sink for daemon/service (rotation policy)
- platform-specific sinks allowed (EventLog on Windows, unified logging on macOS) via DEC

Sinks MUST support:
- bounded buffering
- backpressure behavior
- drop policy (drop oldest/newest) documented via DEC if necessary

---

## 10. Redaction policy (normative)

### 10.1 Classification

Fields MUST be classified as:
- public
- internal
- sensitive (PII/secret)

### 10.2 Redaction rules

- Secrets: drop entirely.
- PII: hash or redact (mask) unless explicit allowlist.
- Large payloads: truncate and include length + hash.

### 10.3 Enforcement

Must have:
- static scan gate for common secret patterns (dependency_policy may also cover)
- unit tests verifying redaction functions
- negative tests ensuring forbidden fields are rejected

---

## 11. Testing and enforcement (normative)

### 11.1 Required tests (baseline)

Projects MUST include tests that:
- validate log schema for a sample event (fields present, types)
- validate redaction (secrets not present)
- validate rate limiting/sampling behavior (if configured)
- ensure debug/trace defaults are off in production builds

### 11.2 CI gates (required)

CI MUST include gates that fail on:
- forbidden strings/keys in emitted logs in tests (e.g., “password=”)
- inclusion of `[TEMP-DBG]` markers
- usage of forbidden logging calls on hotpath (if detectable via grep/clang-tidy rule)
- missing observability budget declarations in Ch1/Ch5 when profile requires them

---

## 12. Default implementation guidance (recommended)

Widely recognized defaults for C++ systems:
- Use a structured logging library or implement a minimal one:
  - `fmt` for formatting (if allowed by dependency policy)
  - JSON encoding optional; line-delimited key/value is acceptable
- Provide compile-time log level stripping via macros:
  - `LOG_TRACE`, `LOG_DEBUG`, etc.
- Provide an injectable clock interface for deterministic testing.

Any dependency choices MUST comply with `dependency_policy.md`.

---

## 13. Minimal example schemas (informative)

### 13.1 Example log event (JSON)

```json
{
  "event": "order_submit_ok",
  "severity": "info",
  "component": "order_router",
  "msg": "Order submitted",
  "ts": 123456789,
  "fields": {
    "order_type": "limit",
    "symbol": "XYZ",
    "qty": 100,
    "latency_microseconds": 42
  }
}
```

### 13.2 Example metric names

- `orders_submitted_count`
- `orders_failed_count`
- `order_submit_latency_microseconds` (histogram recommended)
- `queue_depth_count`

---

## 14. Required blueprint references

Ch1 and Ch5 MUST:
- declare observability budgets (time/memory/log volume),
- identify hotpath observability constraints,
- reference this policy by filename.

Subsystem contracts MUST:
- specify log/metric/trace behaviors and enforcement.

---

## 15. Policy changes

Changes to this policy MUST be introduced via CR and MUST include:
- backward compatibility impact,
- new/updated tests or gates,
- migration guidance for existing log schemas.
