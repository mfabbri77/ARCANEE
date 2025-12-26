<!-- _blueprint/knowledge/cpp_api_abi_rules.md -->

# C++ API/ABI Rules (Normative)
This document provides a **state-of-the-art** baseline for designing **public C++ APIs** and controlling **ABI stability** in native libraries.  
Use these rules when generating Blueprints and when implementing code.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Goals & Definitions
### 1.1 Goals
- Provide a clean, maintainable public API surface.
- Control ABI breakage risk (especially for third-party consumers).
- Preserve performance and safety on the hot path.
- Enable future evolution (deprecations, migrations, feature flags).

### 1.2 Definitions
- **API compatibility:** source-level compatibility (client code still compiles with minor updates).
- **ABI compatibility:** binary-level compatibility (old binaries link/run with new library).
- **Public surface:** everything under `/include` plus any exported symbols.

---

## 2) Choose an ABI Strategy (Mandatory decision)
In the Blueprint, explicitly decide and record under **[DEC-XX]** + **[API-02]**:
- **Strategy A: Best-effort ABI** (recommended default): API stable across minor; ABI may change; consumers recompile.
- **Strategy B: Stable ABI**: ABI guaranteed across minor/patch (higher cost).
- **Strategy C: C ABI boundary**: stable C ABI with C++ wrappers (strongest binary stability).

**Rule:** If stable ABI is required, prefer **PIMPL** or **C ABI boundary**.

---

## 3) Public Headers Design Rules
### 3.1 Include hygiene
- Public headers must include only what they need.
- Avoid including heavy STL headers unless necessary; prefer forward declarations.
- Provide a single top-level include (e.g., `<mylib/mylib.h>`) when appropriate.

### 3.2 Namespace & naming
- Use a single root namespace (e.g., `mylib`).
- Avoid `using namespace` in headers.
- Keep names stable; deprecate rather than rename casually.

### 3.3 Minimize templates on the public surface
- Templates increase compile times and can implicitly expose ABI details.
- If templates are necessary, keep them header-only and stable; avoid exposing internal types.

---

## 4) ABI-Safe Type Guidelines (If ABI matters)
### 4.1 Avoid exposing unstable layouts
Do NOT expose (in stable-ABI mode):
- non-POD structs with private members that may change
- classes with non-final vtables that may evolve
- `std::string`, `std::vector`, `std::function`, `std::optional` in ABI-stable boundaries (stdlib ABI varies)
- `std::shared_ptr` / `std::unique_ptr` as parameters across a stable ABI boundary (ownership semantics + allocator mismatch)

Prefer in ABI-stable boundaries:
- opaque handles (`struct MyHandle; using my_handle_t = MyHandle*;`)
- PIMPL (`class Foo { struct Impl; std::unique_ptr<Impl> p_; };`)
- C-friendly PODs with fixed-size fields and explicit versioning.

### 4.2 Struct evolution rules (public POD)
If you expose public structs:
- Use `uint32_t size` or `uint32_t version` fields for extensibility.
- Append new fields at the end only.
- Document alignment/padding expectations if binary serialization is involved.

### 4.3 Enums
- Prefer `enum class` with explicit underlying type.
- Do not reorder enumerators; only append new ones.
- Reserve ranges if you expect growth.

---

## 5) Function Signatures (Public API)
### 5.1 Parameters and ownership
- For input views: `std::span<const T>`, `std::string_view`, or `{ptr, len}` in C ABI.
- For output buffers: caller-allocated spans or two-call “size then fill” pattern for C ABI.
- Avoid hidden allocations in hot-path calls; document if unavoidable.

### 5.2 Exceptions policy
- Blueprint must decide if exceptions are allowed.
- If exposing a C ABI boundary: functions must be `noexcept` and return error codes/status.

### 5.3 Error types
Recommended patterns:
- C++: `expected<T, Error>` / `StatusOr<T>` style, or `Status` + out-param.
- C ABI: `int` error codes + optional message getter.

**Rule:** Never throw exceptions across ABI boundaries.

---

## 6) Symbol Visibility & Export Macros (Mandatory)
### 6.1 Visibility rules
- Hide symbols by default; export only the public surface.
- GCC/Clang: compile with `-fvisibility=hidden` (where appropriate).
- Use an export macro for public types/functions.

### 6.2 Export macro pattern
Provide a macro like:
- `MYLIB_API` expands to `__declspec(dllexport)` / `__declspec(dllimport)` on Windows
- and to `__attribute__((visibility("default")))` on ELF/Mach-O as needed

**Rule:** The Blueprint must specify this macro and how it is applied consistently.

---

## 7) Versioning in Headers (Mandatory)
### 7.1 Version macros
Define:
- `MYLIB_VERSION_MAJOR`, `MYLIB_VERSION_MINOR`, `MYLIB_VERSION_PATCH`
- optionally a single `MYLIB_VERSION_STRING`

### 7.2 Feature detection
Provide compile-time feature macros, e.g.:
- `MYLIB_HAS_FOO 1`
- `MYLIB_HAS_BAR 0`

Avoid probing symbol presence indirectly when a macro can be used.

---

## 8) Deprecation & Migration (Mandatory)
### 8.1 Deprecation macro
Provide:
- `MYLIB_DEPRECATED(since, removal)` attribute wrapper
- Optionally: `MYLIB_DEPRECATED_MSG("...")`

### 8.2 Deprecation lifecycle
- Deprecate first; keep for at least N minor releases or until next major (per [VER-04]).
- Provide replacements and MIGRATION notes.

**Rule:** Renames and behavior changes must be announced in `CHANGELOG.md` and `MIGRATION.md` when required.

---

## 9) C API Boundary (If selected)
### 9.1 Handle-based design
- All objects are opaque handles.
- Provide create/destroy functions.
- Provide explicit allocator ownership rules.

### 9.2 String and buffers
- Use `{const char* ptr, size_t len}` views for inputs.
- For outputs: caller provides buffer + len; function returns required size.

### 9.3 Thread-safety
- Explicitly document per-handle thread-safety and global initialization/shutdown requirements.

---

## 10) Backward Compatibility Testing (Recommended)
If you claim stable ABI/API across versions, include tests:
- Compile-time tests (API presence).
- Link/run tests against older headers or stub binaries (where feasible).
- Symbol checks (platform dependent).

Blueprint should define [TEST-XX] for these.

---

## 11) Anti-Patterns (Avoid)
- Exposing internal headers/types in `/include`.
- Exporting everything (no visibility control).
- Returning owning raw pointers without clear ownership contract.
- Using exceptions for control flow in hot paths.
- Using `std::cout`/`printf` for logging (use approved logger).
- “Just add a field to a public struct” without versioning/compat plan.

---

## 12) Checklist (Quick)
When finalizing a Blueprint:
- [ ] [API-02] explicitly selects ABI strategy (best-effort / stable / C boundary)
- [ ] Export macro and symbol visibility policy defined
- [ ] Error handling and exception policy explicit
- [ ] Public types are ABI-safe per chosen strategy
- [ ] Deprecation + migration policy aligns with [VER-04]/[VER-05]
- [ ] Compatibility tests defined if guarantees are claimed
