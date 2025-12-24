# Repo Layout & Targets (Normative)
This document standardizes repository layout and build target conventions for native C++ apps/libraries, including optional Python bindings and multi-backend graphics (Vulkan/Metal/DX12).

> **Precedence:** Prompt hard rules → `blueprint_schema.md` → this document → project-specific constraints.

---

## 1) Layout Goals
- Make repos predictable for humans and AI agents.
- Prevent accidental public API leaks.
- Keep build/test/tooling targets consistent across projects.
- Support multiple backends cleanly without spaghetti dependencies.

---

## 2) Canonical Directory Layout
```
/blueprint/                 # source-of-truth specs
/cmake/                     # CMake modules/toolchains/helpers
/include/<proj>/            # public headers ONLY
/src/                       # private implementation
/src/<proj>/                # recommended subdir for core implementation
/src/backends/              # backend implementations (vulkan/metal/dx12)
/tests/                     # tests (unit/integration/stress)
/examples/                  # sample usage (optional)
/tools/                     # scripts + small dev tools
/docs/                      # changelog/migration/architecture docs
/third_party/               # only if vendoring is necessary (discouraged)
```

**Rule:** Public headers must live exclusively in `/include/<proj>/`.

---

## 3) Modules & Target Topology (Recommended)
### 3.1 Core vs adapters
- `<proj>`: core library (public API + thin dispatch)
- `<proj>_core`: optional internal target if you want to separate exported API from core code
- `<proj>_platform`: small platform abstraction layer (fs, threading, time, etc.) if needed

### 3.2 Backend targets (graphics)
Prefer separate backend targets:
- `<proj>_vk` (Vulkan backend)
- `<proj>_mtl` (Metal backend)
- `<proj>_dx12` (DX12 backend)

Rules:
- Backends depend on core, not vice versa (core must not include backend headers).
- Backends should be selected via factory/registry in core behind an interface.

### 3.3 Optional utility targets
- `<proj>_cli`: command-line tool using the library (if useful)
- `<proj>_tools`: internal dev tools (profiling, asset conversion, etc.)
- `<proj>_py`: Python extension module target (if applicable)

---

## 4) Public vs Private Headers (Mandatory)
### 4.1 Public headers
- Only stable API types/functions.
- No backend-specific headers in public surface unless explicitly intended.
- No “accidental” exposure of internal dependencies.

### 4.2 Private headers
- Live under `/src/` (or `/src/<proj>/`) and are not installed.
- May include backend details, platform glue, private data layouts.

**Rule:** Public headers should compile independently (include what they use).

---

## 5) Include Path Rules (Mandatory)
- `<proj>` target:
  - `PUBLIC`: `/include`
  - `PRIVATE`: `/src`
- No global include directories (avoid `include_directories()`).
- Use `target_include_directories()` with BUILD/INSTALL interface for libraries.

---

## 6) Install/Export Rules (Libraries)
If the project is a library/SDK:
- Install public headers to `include/<proj>/`
- Install targets and export CMake package config
- Do not install private headers or backend implementation details unless explicitly part of the public SDK.

---

## 7) Tests Layout & Targets
Suggested structure:
```
/tests/
  unit/
  integration/
  stress/
  data/
```

Targets:
- `<proj>_tests_unit`
- `<proj>_tests_integration`
- `<proj>_tests_stress` (optional)
Or a single `<proj>_tests` with CTest labels.

Rules:
- Tests link to `<proj>` (public surface) and may link to backends only if they are integration tests.
- Unit tests should prefer testing through public API or internal test hooks explicitly designed.

---

## 8) Examples & Tools
- `/examples` should demonstrate minimal, idiomatic usage and be kept stable.
- `/tools` should contain scripts and small binaries not shipped as part of the public library.
- Any tool that is shippable should live as a proper target (`<proj>_cli`) and be documented.

---

## 9) Python Bindings Layout (If applicable)
Recommended:
```
/python/
  pyproject.toml (or setup.cfg)
  /<proj>/
    __init__.py
/src/python/            # C++ binding code (nanobind/pybind11)
```

Targets:
- `<proj>_py` (extension module)
Rules:
- Keep binding layer thin; no business logic in Python glue.
- Ensure packaging and tests are part of CI.

---

## 10) Cross-Platform Backend Notes
### 10.1 Vulkan
- Prefer dynamic loader integration strategy documented in Blueprint.
- Backend should compile on all platforms that support it (or be conditionally built).

### 10.2 Metal
- Keep Objective-C++ (.mm) files isolated under the Metal backend target.
- Avoid leaking Objective-C++ into core.

### 10.3 DX12
- Keep Windows-specific includes and COM helpers inside DX12 backend target.
- Avoid polluting core with Windows headers.

---

## 11) Mandatory Quality Gate Targets
Repos must provide these build targets (names may be standardized in CMake playbook):
- `check_no_temp_dbg`
- `format` and `format_check`
- `lint` (recommended)
- tests via CTest presets (mandatory)

---

## 12) Compliance Checklist
- [ ] Public headers only in `/include/<proj>/`
- [ ] Core target does not depend on backend targets
- [ ] Backend targets are separable and conditionally buildable
- [ ] Tests organized and runnable via CTest labels or clear targets
- [ ] Python bindings isolated and thin (if present)
- [ ] Required quality gate targets exist
