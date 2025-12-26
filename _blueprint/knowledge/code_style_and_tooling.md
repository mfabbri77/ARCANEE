<!-- code_style_and_tooling.md -->

# Code Style and Tooling (Normative)

This document defines the **code style**, formatting, linting, static analysis, and developer tooling expectations for repositories governed by the DET CDD blueprint system.

It is **normative** unless overridden via DEC + enforcement.

---

## 1. Goals

- Consistent, readable codebase across platforms.
- Enforced style via automation (CI gates).
- Minimize subjective style debates.
- Promote correctness (lint, sanitizers, analysis).
- Keep tooling deterministic and reproducible.

---

## 2. Core rules (MUST)

### 2.1 Formatting is automated

- The repo MUST provide a formatting configuration (default: `.clang-format`).
- CI MUST enforce formatting:
  - either a format-check job (`clang-format --dry-run` equivalent),
  - or a gate integrated into the validator.

Manual formatting rules are non-authoritative; automation is authoritative.

### 2.2 Lint/static analysis is automated

- The repo MUST provide lint configuration (default: `.clang-tidy`).
- CI MUST run lint on at least one OS (Linux recommended).

Windows analysis:
- run MSVC `/analyze` or equivalent as per `ci_reference.md`.

### 2.3 Determinism and hotpath constraints

In performance/determinism profiles:
- avoid hidden allocations on hotpath (DEC if allowed)
- avoid logging on hotpath unless explicitly bounded
- enforce via benchmarks and targeted lint rules where feasible

---

## 3. Widely recognized defaults (recommended baseline)

### 3.1 clang-format style defaults

Default base:
- `BasedOnStyle: LLVM`
- 2-space indentation (or 4-space; choose one and enforce)
- column limit: 100 (reasonable default)
- pointer alignment: `Left` (common C++ style)
- keep includes sorted

A project MAY choose Google style or another, but MUST document via DEC if deviating from baseline.

### 3.2 clang-tidy checks (baseline)

Enable a pragmatic subset:
- `bugprone-*`
- `clang-analyzer-*`
- `modernize-*` (careful with ABI)
- `performance-*`
- `readability-*` (selectively)
- `cppcoreguidelines-*` (selectively; avoid overly noisy checks)

Avoid enabling extremely noisy rules without a plan; decisions must be enforceable.

### 3.3 EditorConfig (recommended)

Provide `.editorconfig` for whitespace consistency:
- LF line endings (or consistent handling)
- trailing whitespace trimming
- final newline

---

## 4. C++ style rules (normative guidance)

### 4.1 Naming conventions (default)

- Types: `PascalCase`
- Functions: `camelCase` (or `snake_case` — pick one and enforce)
- Variables: `snake_case`
- Constants: `kPascalCase` or `SCREAMING_SNAKE_CASE` (pick one)
- Macros: `SCREAMING_SNAKE_CASE`

The project MUST pick a consistent convention and encode it in lint/config where feasible. If multiple conventions exist, record via DEC.

### 4.2 Headers and includes

- Public headers in `/include/<project>/...`.
- Public headers MUST be self-contained (include what they use).
- Prefer forward declarations where appropriate, but not at cost of correctness.
- Includes SHOULD be ordered:
  1) corresponding header
  2) standard library
  3) third-party
  4) project headers

### 4.3 Error handling consistency

Code MUST follow the selected error model (see `error_handling_playbook.md`):
- do not mix exceptions and status codes arbitrarily
- map errors to stable error codes/messages

### 4.4 Ownership and lifetimes

Code MUST follow the API/lifetime policy (see `cpp_api_abi_rules.md`):
- explicit ownership semantics
- avoid raw owning pointers unless documented
- avoid reference-to-temporary hazards

### 4.5 Concurrency discipline

When concurrency exists:
- avoid data races (TSan gates)
- prefer standard primitives (`std::mutex`, `std::atomic`, etc.) unless DEC approves custom lock-free structures.

---

## 5. Tooling catalog (normative expectations)

### 5.1 Required tools

- clang-format (or equivalent formatter)
- clang-tidy (or equivalent linter)
- CMake formatting optional (cmake-format) via DEC
- Python 3 (if scripts are used, pinned requirements recommended)

### 5.2 Optional tools

- include-what-you-use
- clangd configuration
- codespell (docs)

Optional tools MUST be encoded as CI jobs or documented local commands.

---

## 6. CI gates (MUST)

CI MUST fail on:
- formatting violations
- lint violations (or a defined allowlist with expiration via DEC)
- new warnings when warnings-as-errors is enabled
- missing tooling configs when required (e.g., .clang-format absent)

If a project must temporarily suppress a lint rule:
- record DEC or CR
- add expiry mechanism (e.g., `TODO` tag plus a gate date) if feasible

---

## 7. Version pinning and reproducibility (recommended)

To reduce drift:
- pin tool versions via:
  - CI runner images, or
  - toolchain scripts, or
  - container images

If using containers:
- document in Ch7 and provide local commands.

---

## 8. Minimal example configs (informative)

### 8.1 Example `.clang-format`

```yaml
BasedOnStyle: LLVM
IndentWidth: 2
ColumnLimit: 100
PointerAlignment: Left
SortIncludes: true
```

### 8.2 Example `.clang-tidy` snippet

```yaml
Checks: >
  clang-analyzer-*,
  bugprone-*,
  performance-*,
  modernize-*,
  readability-*
WarningsAsErrors: '*'
```

(Real configs should tune readability/cppcoreguidelines to avoid noise.)

---

## 9. Policy changes

Changes to this policy MUST be introduced via CR and MUST include:
- updates to CI gates and validator behavior,
- migration steps for existing repos.
