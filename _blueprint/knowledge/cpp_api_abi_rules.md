<!-- cpp_api_abi_rules.md -->

# C++ API / ABI Rules (Normative)

This document defines the **public API design rules**, ABI stability constraints, and cross-platform interface conventions for C++17/20/23 systems governed by the DET CDD blueprint framework.

It is **normative** unless superseded by higher-precedence rules via DEC + enforcement.

---

## 1. Goals

- Provide stable, predictable public APIs.
- Avoid accidental ABI breaks across platforms/compilers/stdlibs.
- Make ownership, lifetimes, threading, and error behavior explicit.
- Support cross-platform compilation and packaging.
- Enable deterministic evolution via SemVer and blueprint overlays.

---

## 2. Definitions

- **Public API**: headers and symbols intended for external consumption.
- **ABI**: binary interface: symbol names, calling conventions, type layouts, vtables, exception behavior.
- **API break**: source-level breaking change.
- **ABI break**: binary-level breaking change (may or may not be source-breaking).
- **Header-only**: library delivered entirely as headers; ABI concerns are reduced but still exist for inline code changes and ODR.

---

## 3. Public surface rules (MUST)

### 3.1 Public headers are explicit and minimal

- Public headers MUST live under `/include/<project>/...`.
- Public headers MUST be documented in Ch4.
- Anything not intended for public use MUST NOT be installed/exported.

### 3.2 Ownership and lifetimes are explicit

Every public API MUST specify:
- who owns returned pointers/references,
- lifetime constraints of references/views,
- whether objects can be shared across threads,
- destruction responsibility.

Preferred patterns:
- return values for ownership
- `std::unique_ptr<T>` for owned heap objects (if ABI permits; see §6)
- `std::shared_ptr<T>` only when shared ownership is required (document overhead)
- non-owning views:
  - `std::span`, `std::string_view` (only when lifetime is well-defined)

### 3.3 Threading/synchronization contract

Public APIs MUST declare:
- thread-safety level:
  - thread-safe
  - thread-compatible
  - not thread-safe
- which methods may be called concurrently
- reentrancy constraints
- synchronization responsibilities (caller vs callee)

### 3.4 Error model is explicit

Public APIs MUST:
- declare whether they throw or return errors,
- declare error codes/categories (see error_handling_playbook.md),
- avoid mixing models without DEC.

### 3.5 Determinism and performance contracts

If determinism/perf budgets exist:
- public APIs MUST specify:
  - time source behavior (if exposed)
  - allocation behavior on hotpath
  - logging behavior on hotpath
  - complexity bounds (Big-O or explicit constraints)

---

## 4. Symbol visibility and export macros (MUST)

### 4.1 Export macro policy

If building shared libraries, the project MUST define export macros:

- `MYPROJ_API` (or equivalent)
- `MYPROJ_LOCAL` (optional)

Rules:
- On Windows:
  - use `__declspec(dllexport)` when building the DLL
  - use `__declspec(dllimport)` when consuming
- On GCC/Clang:
  - use `__attribute__((visibility("default")))` as needed

Ch4 MUST define:
- macro naming
- where macros live (public header)
- how to build static vs shared

### 4.2 Namespace policy

- Public APIs MUST use a stable namespace.
- Avoid exposing internal namespaces like `detail` unless explicitly intended.

---

## 5. ABI stability rules (conditional, but often MUST)

Projects MUST declare an ABI policy in Ch4:
- “header-only”
- “ABI-stable” (strong guarantees)
- “ABI-best-effort” (common for internal libs)
- “no ABI guarantees” (application-only)

If any consumers rely on ABI stability, use “ABI-stable” or “ABI-best-effort” and enforce by gates.

### 5.1 ABI-stable guidelines (recommended when stable ABI required)

To reduce ABI breaks:
- avoid exposing STL types across DLL boundaries on Windows unless you control toolchain/CRT
- use PImpl for classes with stable ABI needs
- avoid inline function bodies in public headers when ABI stability is required
- avoid exposing templates as exported symbols (prefer type-erasure or C wrappers)

### 5.2 ABI break classification

The following changes are ABI-breaking:
- changing struct/class layout (adding/reordering members)
- changing virtual function order
- changing exception specification that affects ABI (toolchain-dependent)
- changing exported symbol names
- changing calling convention
- changing enum underlying values if used in ABI

ABI breaks MUST be reflected in SemVer bump policy (usually MAJOR) unless explicitly internal-only.

---

## 6. Types allowed in public API (defaults)

### 6.1 Safe-by-default types (recommended)

- fixed-width integers: `std::uint32_t` etc.
- plain enums with explicit underlying type
- POD structs with explicit layout (careful with padding)
- opaque handles (pointer-to-incomplete type)
- `std::span` / `std::string_view` (when lifetime is stable and callers are C++20)

### 6.2 Types requiring caution / DEC

- `std::string`, `std::vector`, and other STL containers across shared library boundary (Windows especially)
- `std::function` (allocation + ABI)
- exceptions across boundaries
- custom allocators exposed publicly

If used, DEC MUST document:
- toolchain coupling assumptions
- ABI expectations per platform
- tests/gates ensuring compatibility

### 6.3 C ABI wrapper option (recommended when stable ABI required)

For maximum ABI stability:
- provide a C API wrapper:
  - `extern "C"` functions
  - opaque handles
  - explicit ownership functions

This approach should be decided via DEC if consumers require stable ABI across compilers.

---

## 7. Header content rules (normative defaults)

### 7.1 Declaration-only public headers (default)

Public headers SHOULD be declaration-only:
- templates/constexpr/inline trivial code is acceptable
- substantial implementations in headers require DEC and justification

Rationale:
- reduces rebuild costs
- reduces ABI exposure
- improves encapsulation

### 7.2 Include guards

Use either:
- `#pragma once` (widely supported)
- or traditional include guards

Choose one and enforce.

### 7.3 Exceptions and RTTI policy

Ch4 MUST declare:
- exceptions enabled/disabled
- RTTI enabled/disabled
- and how this differs per platform

CI SHOULD verify flags are consistent across presets.

---

## 8. Versioning and deprecation (normative)

### 8.1 Deprecation mechanism

- Use `[[deprecated("message")]]` for deprecations when possible.
- Provide migration notes in release.md.
- Deprecations SHOULD exist for at least one minor release before removal (unless security/critical fix).

### 8.2 SemVer mapping

Default mapping:
- PATCH: bugfixes, no API/ABI breaks
- MINOR: additive API changes, compatible behavior
- MAJOR: API breaks or ABI breaks

If ABI guarantees are “none”, SemVer may be more flexible, but must be documented in Ch9 via DEC.

---

## 9. Testing and enforcement (MUST)

### 9.1 API contract tests

Tests MUST verify:
- ownership and lifetime invariants (where possible)
- error model behavior (returned codes, no-throw policy)
- thread-safety claims (stress tests or TSan where feasible)

### 9.2 ABI checks (conditional)

If ABI stability is promised:
- CI MUST include an ABI compatibility check on at least one platform (Linux recommended) using tools such as:
  - `abi-compliance-checker` / `abi-dumper` (if appropriate)
- Windows ABI checks are harder; may use symbol diffing and strict toolchain pinning.

Any ABI policy enforcement tool choice requires DEC if nontrivial.

---

## 10. Interactions with GPU APIs (conditional)

If TOPIC=gpu, the public API must avoid leaking backend-specific handles unless explicitly intended.  
See `GPU_APIs_rules.md` and `multibackend_graphics_abstraction_rules.md`.

---

## 11. Policy changes

Changes to this policy MUST be introduced via CR and MUST include:
- migration notes,
- updated tests/gates.
