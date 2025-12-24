# Code Style & Tooling Policy (clang-format / clang-tidy) — Normative
This document standardizes code style, formatting, and static analysis for native C++ repositories to keep output consistent across AI agents and over the software lifecycle.

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Goals
- Keep code consistent and reviewable (minimal style drift).
- Catch common correctness issues early (lint + sanitizers).
- Reduce include/ABI surprises (header hygiene).
- Make tooling runnable locally and in CI.

---

## 2) Canonical Tools (Mandatory)
- **Formatting:** clang-format
- **Static analysis:** clang-tidy (primary), optional cppcheck
- **Build system:** CMake + presets (see `cmake_playbook.md`)
- **Compiler database:** `compile_commands.json` must be enabled in dev/ci

**Rule:** Tool versions should be pinned or documented (e.g., “clang-format 17+”).

---

## 3) clang-format Policy (Mandatory)
### 3.1 Configuration
- Provide a repo `.clang-format` at the root.
- Choose a base style (LLVM or Google) and document it in the Blueprint ([Appendix A]).
- Avoid frequent churn: do not reformat entire repo unless planned.

### 3.2 Formatting commands
- Provide:
  - `format` target (applies formatting)
  - `format_check` target (fails if formatting changes would occur)

### 3.3 Generated files
- Do not format generated code unless it is checked in and intended for review.

---

## 4) clang-tidy Policy (Recommended, often mandatory in CI)
### 4.1 Configuration
- Provide `.clang-tidy` at repo root.
- Use `clang-tidy` via a CMake target `lint` where possible.

### 4.2 Baseline checks (recommended set)
Start with a pragmatic baseline to avoid excessive noise:
- `bugprone-*`
- `performance-*`
- `readability-*` (selectively)
- `modernize-*` (selectively; avoid noisy rules)
- `clang-analyzer-*`

Avoid or carefully gate:
- aggressive modernize rules that cause churn
- rules that conflict with your ABI policy

**Rule:** If a check causes high false positives, disable it and document why in the Blueprint.

### 4.3 Warnings policy
- CI may enforce “no new clang-tidy warnings” rather than “zero warnings immediately”.
- Prefer incremental adoption:
  - baseline existing warnings
  - fail CI only on new warnings

---

## 5) Naming Conventions (Mandatory baseline)
Choose one style guide (LLVM or Google) and apply consistently. Recommended baseline:
- Namespaces: `lower_snake_case` or `lowercase` (project choice)
- Types (classes/structs/enums): `PascalCase`
- Functions/methods: `camelCase` or `snake_case` (choose and codify)
- Constants: `kConstantName` (Google) or `kConstant_name` (project-defined)
- Files: `snake_case.cpp/.h` or `PascalCase.h` (choose and codify)

**Rule:** The Blueprint must state the chosen convention; agents must follow it.

---

## 6) Header Hygiene (Mandatory)
### 6.1 Public headers
- Public headers must be self-contained and compile on their own.
- Avoid including backend headers in public API (unless intended).
- Avoid `using namespace` in headers.

### 6.2 Include order (recommended)
- own header first
- standard library
- third-party
- project headers

### 6.3 Forward declarations
- Prefer forward declarations in headers to reduce compile times when safe.
- Do not forward-declare standard library types incorrectly (use real includes where required).

---

## 7) Include-What-You-Use (Optional but valuable)
If adopting IWYU:
- define IWYU mapping files if needed
- integrate as a separate CI job
- avoid churn by incremental cleanup

---

## 8) Warnings & “Clean Build” Policy
- Use strong warnings; CI uses Werror as per `cmake_playbook.md`.
- Warnings in third-party code should not break CI:
  - avoid compiling deps with your Werror flags unless you control them
  - isolate warnings flags to your targets

**Rule:** The project’s own code must build warning-free in CI.

---

## 9) C++ Language Rules (Recommended)
### 9.1 Exceptions
- Follow the Blueprint’s exception policy strictly ([API-03]).
- Never throw across ABI boundaries.

### 9.2 RTTI
- Decide in Blueprint whether RTTI is allowed.
- If disabled, enforce consistently.

### 9.3 `noexcept`
- Use `noexcept` when it is part of the API contract or improves optimization.
- Do not mark functions `noexcept` if they can throw under policy.

### 9.4 `[[nodiscard]]`
- Use `[[nodiscard]]` on status/expected return types to prevent ignored errors.

---

## 10) Comments & Traceability (Mandatory)
- Complex algorithms must include a comment referencing Blueprint IDs, e.g.:
  - `// Implements [CONC-02]: synchronization rules ...`
- Avoid verbose comments that restate obvious code; focus on invariants and contracts.

---

## 11) Tooling Targets (Mandatory)
Repository should provide:
- `format`
- `format_check`
- `lint` (recommended)
- `check_no_temp_dbg` (mandatory per `temp_dbg_policy.md`)

---

## 12) CI Integration (Mandatory)
CI must run:
- `format_check`
- `check_no_temp_dbg`
- build + tests
- sanitizers (at least on Linux)

Recommended:
- `lint` (blocking or “no new warnings” mode)

---

## 13) Anti-Patterns (Avoid)
- Reformatting huge portions without cause.
- Ignoring lint warnings by disabling broad check groups without rationale.
- Adding style exceptions ad-hoc; codify any exception in this file and Blueprint.
- Allowing public headers to include platform/backend headers unintentionally.

---

## 14) Quick Checklist
- [ ] `.clang-format` exists and `format_check` is a CI gate
- [ ] `.clang-tidy` exists and `lint` is available (CI policy defined)
- [ ] Naming conventions chosen and documented
- [ ] Public headers are self-contained and clean
- [ ] Traceability comments reference Blueprint IDs for complex logic
- [ ] Tool versions pinned/documented
