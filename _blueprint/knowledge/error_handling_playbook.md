<!-- error_handling_playbook.md -->

# Error Handling Playbook (Normative)

This playbook defines the **error model**, error taxonomy, propagation rules, and enforcement expectations for C++ systems governed by the DET CDD blueprint framework.

It is **normative** unless overridden via DEC + enforcement.

---

## 1. Goals

- Provide consistent, predictable error behavior across subsystems.
- Make error contracts explicit in APIs (Ch4 subsystem contracts).
- Support performance and determinism constraints (AAA/HFT).
- Ensure errors are testable, observable, and diagnosable.
- Preserve ABI stability where relevant.

---

## 2. Baseline error model (default)

### 2.1 Default choice (widely recognized, rational)

Default model for libraries and services:
- Use **status/result return values** for expected failures.
- Use **exceptions** only for:
  - programmer errors (optional),
  - truly exceptional, unrecoverable conditions,
  - or when an API boundary already uses exceptions and performance constraints allow it.

Because AAA/HFT often avoids exceptions for predictability:
- `PROFILE_AAA_HFT` default is **no exceptions across public API boundaries** (DEC required to allow).

A project MUST confirm the chosen model via DEC in decision log when non-trivial.

### 2.2 Guiding principle

- **Expected** errors: represented explicitly in return type (no throws).
- **Unexpected** errors: assertions, terminate, or exception based on policy (DEC).

---

## 3. Error taxonomy (normative)

### 3.1 Error categories

Define a stable set of categories, e.g.:
- `invalid_argument`
- `not_found`
- `already_exists`
- `permission_denied`
- `resource_exhausted`
- `deadline_exceeded`
- `unavailable`
- `internal`
- `not_implemented`

Projects MAY customize categories, but MUST remain stable across versions unless SemVer MAJOR change.

### 3.2 Error codes

- Each error MUST have a stable **error code** string or integer.
- Error codes MUST be:
  - stable across patch/minor releases,
  - only removed/renamed in MAJOR releases (or via explicit deprecation).

Recommended pattern:
- string codes: `COMPONENT_ERRORNAME` (e.g., `NET_TIMEOUT`)
- numeric codes: enum class with explicit values (ABI-safe)

### 3.3 Error payload

Error payload SHOULD include:
- code
- human-readable message (optional)
- optional context fields (safe to log; redacted as needed)

Avoid embedding large payloads.

---

## 4. API contract requirements (normative)

Every public API (Ch4) MUST specify:
- which errors can occur (by code/category)
- whether errors are returned or thrown
- whether errors are recoverable
- whether errors are deterministic (same input → same error)
- threading implications (errors must not depend on races)

### 4.1 Return type guidance

Preferred patterns:

- `Expected<T, Error>`-like (custom or `tl::expected`/`std::expected` when available)
- `Status` + out-parameter (for ABI constraints)
- `std::optional<T>` only when “no value” is the only failure signal and is unambiguous

If ABI stability is critical, avoid templates in public ABI (DEC required if using).

---

## 5. Exceptions policy (normative)

### 5.1 Public API boundaries

Default:
- Libraries: do not throw exceptions across public boundaries unless DEC.
- Applications: may use exceptions internally, but must handle at boundary.

### 5.2 Exceptions and performance/determinism

In `PROFILE_AAA_HFT`:
- exceptions SHOULD be disabled for production builds where feasible
- errors must be handled via return values
- stack unwinding overhead and unpredictability is unacceptable on hotpath

If exceptions are enabled:
- document in DEC with benchmarks/gates verifying overhead is within budget.

### 5.3 No-throw guarantees

Where possible, APIs SHOULD declare `noexcept` when they cannot fail and are performance-sensitive.

---

## 6. Logging and observability integration (normative)

- Error logs MUST include:
  - error code
  - component
  - correlation_id if available
- Error logs MUST NOT leak sensitive data (see logging policy).
- For repeated errors, rate limit logs to avoid flooding.

---

## 7. Testing requirements (normative)

Projects MUST include:
- unit tests covering each error category used by a subsystem
- negative tests for invalid inputs
- tests verifying error determinism when required:
  - same input yields same error code/message (message may allow variable context)
- tests verifying exceptions are not thrown across boundaries when forbidden

---

## 8. Mapping and translation (normative)

When crossing boundaries (e.g., OS errors, network library errors):
- translate into the project error taxonomy
- preserve relevant context safely
- avoid leaking raw errno strings in public APIs (may log internally)

---

## 9. ABI considerations (conditional)

If the project exposes a stable ABI:
- public ABI MUST avoid:
  - throwing exceptions across boundary,
  - types with unstable layout across compilers/stdlib (e.g., `std::string` across DLL boundary in MSVC requires care),
  - templates as exported ABI (unless header-only).

See `cpp_api_abi_rules.md` for detailed ABI policy.

---

## 10. Widely used patterns (informative)

### 10.1 Status type sketch (conceptual)

- `struct Status { ErrorCode code; std::string msg; }`
- `StatusOr<T>` / `Expected<T, Status>`

Prefer `std::error_code` style if applicable, but ensure stable codes.

---

## 11. Enforcement gates (summary)

CI/validator SHOULD fail if:
- APIs lack declared error model (Ch4 contract incomplete)
- forbidden exception usage is detected in public headers (lint rule)
- error codes are changed without SemVer bump/deprecation plan
- tests for negative/error paths are missing when required

---

## 12. Policy changes

Changes to this playbook MUST be introduced via CR and MUST include:
- migration steps for existing APIs,
- updated tests/gates reflecting new error model.
