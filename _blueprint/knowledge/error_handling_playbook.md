<!-- _blueprint/knowledge/error_handling_playbook.md -->

# Error Handling Playbook (Normative)
This document standardizes **error handling** across native C++ codebases (libraries/apps), including **API/ABI boundaries**, **Python bindings**, and multi-backend graphics.  
The goal is consistent behavior, predictable performance, and clean diagnostics.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Goals
- Make error behavior explicit and testable.
- Prevent exceptions or undefined behavior crossing ABI boundaries.
- Keep hot paths fast (avoid allocations and heavy formatting in tight loops).
- Provide high-quality diagnostics (structured logs + error codes/messages).
- Ensure Python bindings map errors cleanly to exceptions.

---

## 2) Choose an Error Strategy (Mandatory Blueprint decision)
In the Blueprint, decide under **[API-03]** and record rationale under **[DEC-XX]**:

### Strategy A — Exceptions enabled (C++-only surface)
- Use exceptions internally and/or in public C++ API.
- Still **must not** throw across C ABI boundaries.

### Strategy B — No exceptions (recommended default for SDKs/engines)
- Compile with exceptions disabled where feasible.
- Use `Status`/`expected<T, Error>` patterns.

### Strategy C — C ABI boundary
- All exported functions are `noexcept` and return error codes/status handles.

**Rule:** Pick exactly one primary strategy for the public surface. Internal code may still use localized exceptions only if policy permits and they never cross module boundaries.

---

## 3) Canonical Types (Recommended)
### 3.1 ErrorCode
Define a stable `enum class ErrorCode : uint32_t` with explicit underlying type.
Rules:
- Never reorder values; only append.
- Reserve ranges per subsystem if helpful (e.g., 1000–1999 IO).

### 3.2 Error
A lightweight error struct:
- `ErrorCode code`
- optional `uint32_t subcode` (backend-specific)
- optional fixed-size message buffer or message pointer owned by error system
- optional context fields (file/line only in debug builds if overhead matters)

### 3.3 Status / expected
Preferred options:
- `Status` for operations returning no value
- `Expected<T>` / `expected<T, Error>` for value-or-error

**Hot-path rule:** avoid heap allocations on failure paths that may be frequent; prefer fixed buffers or interned messages.

---

## 4) Public API Rules (Mandatory)
### 4.1 Never throw across boundaries
- If exposing a C ABI or stable ABI boundary: exported functions must be `noexcept`.
- Convert internal failures into error codes/status.

### 4.2 Ownership of error messages
Blueprint must define one of:
- message is copied into caller buffer (`get_last_error_message(buf, len)`)
- message is stored in thread-local “last error” (simple, but must be documented)
- message is returned as `{ptr,len}` with explicit lifetime rules

**Rule:** The lifetime rules must be explicit and testable.

### 4.3 Returning errors vs logging
- Return errors to the caller; logging is supplementary.
- Do not log at both layers by default (avoid duplicate logs). Define a rule:
  - libraries return errors; apps decide how/when to log.

---

## 5) Logging Integration (Tied to [REQ-02])
- Use the approved logger (spdlog/glog) only.
- Prefer structured logging: `event=... code=... backend=...`.
- Avoid heavy formatting on hot paths unless the log level is enabled.
- Never use `std::cout`/`printf` for operational logging.

**Rule:** Error objects must be convertible to a short string for logging, but formatting must be lazy or guarded by log level.

---

## 6) Graphics Backend Error Mapping (Vulkan/Metal/DX12)
Blueprint must define how backend errors map to canonical errors:

### 6.1 Vulkan
- Map `VkResult` to `ErrorCode` + `subcode = VkResult`.
- Preserve `VK_ERROR_DEVICE_LOST` distinctly.
- Validation-layer messages are diagnostics, not stable API errors.

### 6.2 Metal
- Map Objective-C/NS errors to canonical `ErrorCode`.
- Avoid leaking Obj-C types into core; convert in Metal backend.

### 6.3 DX12
- Map `HRESULT` to `ErrorCode` + `subcode = HRESULT`.
- If using DRED/Debug Layer, treat output as diagnostics.

**Rule:** Canonical errors should be backend-agnostic where possible (e.g., `DeviceLost`, `OutOfMemory`, `InvalidArgument`).

---

## 7) Python Bindings Error Mapping (If applicable)
- Map canonical errors to Python exceptions:
  - `InvalidArgument` → `ValueError`
  - `OutOfMemory` → `MemoryError`
  - `NotFound` → `KeyError` or `FileNotFoundError`
  - `PermissionDenied` → `PermissionError`
  - fallback → `RuntimeError`
- Include error code and short message in exception text.
- Ensure no C++ exceptions escape the binding layer unless explicitly intended by policy.

**Rule:** Binding layer must be thin; it should not invent new error semantics.

---

## 8) “Last Error” Pattern (If used)
If using thread-local last-error:
- Each thread has its own last error.
- Define:
  - how it is set/cleared
  - whether it is overwritten on success
  - how callers retrieve it
- Provide tests ensuring correctness under concurrency.

**Caution:** last-error is convenient but can hide errors if callers ignore return values. Prefer explicit `Status/Expected` where feasible.

---

## 9) Testing Requirements (Mandatory)
Blueprint must include [TEST-XX] entries for:
- error code mapping tests (backend-specific mapping tables)
- boundary tests: ensure no exceptions cross ABI boundary
- message lifetime tests (buffers/last-error)
- Python exception mapping tests (if bindings exist)
- negative tests for invalid inputs (InvalidArgument)

---

## 10) Anti-Patterns (Avoid)
- Using exceptions for routine control flow on hot paths.
- Returning owning raw pointers on error paths without clear ownership.
- Logging the same error at multiple layers by default.
- Encoding backend-specific errors directly in public API types.
- “Stringly-typed” errors as the only contract (strings are unstable).

---

## 11) Quick Checklist
- [ ] [API-03] error strategy selected and justified via [DEC-XX]
- [ ] Canonical ErrorCode/Error types defined (or explicitly N/A for app-only)
- [ ] ABI boundary is noexcept and non-throwing (if applicable)
- [ ] Vulkan/Metal/DX12 errors mapped consistently (subcode preserved)
- [ ] Python exceptions mapping defined and tested (if applicable)
- [ ] Tests cover error paths and message lifetimes
