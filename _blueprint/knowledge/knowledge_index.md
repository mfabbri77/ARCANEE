<!-- knowledge_index.md -->

# Knowledge Files Index (DET CDD ARCH AGENT)

This file is an **index + onboarding guide** for the knowledge set required by the DET CDD ARCH AGENT system prompt.

Recommended placement:
- `/_blueprint/_knowledge/` (read-only references)
- `/_blueprint/rules/` (enforceable rule files used by validator/CI)

If you keep a strict separation:
- Put “schema/templates/playbooks/policies” in `/_blueprint/_knowledge/`
- Put “CI-enforced rule definitions” in `/_blueprint/rules/`
Some orgs keep everything in `_knowledge` and reference from `rules/` via symlinks; either is acceptable if the validator resolves paths deterministically.

---

## 1. Normative precedence (summary)

Highest to lowest:
1. System prompt HARD rules
2. `prompt meta rules v1 1.md`
3. `blueprint_schema.md`
4. Most-specific rulefile (topic/platform)
5. User constraints
6. Fallback DEC (must be enforced by tests/gates)

---

## 2. Required knowledge files (catalog)

### 2.1 Schema and meta-operation
- `prompt meta rules v1 1.md`  
  Agent operation rules: mode selection, strict 2-phase, emission protocol, sharp questions.

- `blueprint_schema.md`  
  Canonical blueprint schema: Ch0..Ch9 requirements, ID rules, overlay composition, release schema, validator expectations.

- `implementation_checklist_schema.md`  
  YAML schema for baseline checklist and overlay checklist deltas, including traceability requirements.

### 2.2 Templates
- `decision_log_template.md`  
  Required structure for decision logs (`decision_log.md` and `decision_delta_log.md`).

- `cr_template.md`  
  Required structure for change requests (`CR-XXXX.md`) and enforcement expectations.

- `walkthrough_template.md`  
  Required structure for baseline walkthrough and delta walkthrough.

### 2.3 Repo and CI reference
- `repo_layout_and_targets.md`  
  Canonical repo layout and required build targets/structure.

- `cmake_playbook.md`  
  CMake + presets standards, toolchain/preset expectations, deterministic build rules.

- `ci_reference.md`  
  Minimum CI matrix and required gates; reference job structure and gate catalog.

### 2.4 Core engineering policies
- `dependency_policy.md`  
  Dependency selection, pinning, licenses, security scanning, SBOM guidance.

- `testing_strategy.md`  
  Test taxonomy, required tests by topic/profile, fuzzing/perf requirements, CI integration.

- `code_style_and_tooling.md`  
  Formatting/lint/static analysis expectations and CI enforcement.

- `error_handling_playbook.md`  
  Error taxonomy, return-vs-exception model, mapping rules, tests and gates.

- `logging_observability_policy_new.md`  
  Structured logging/metrics/tracing, redaction rules, budgets, determinism-aware behavior.

- `temp_dbg_policy.md`  
  Temporary debug marker policy (`[TEMP-DBG]`) and mandatory CI fail conditions.

- `performance_benchmark_harness.md`  
  Benchmark harness and performance regression policy when budgets exist.

### 2.5 GPU-specific policies (conditional on TOPIC=gpu)
- `GPU_APIs_rules.md`  
  Backend-specific rules for Vulkan/Metal/DX12 plus cross-cutting GPU contracts and CI strategy.

- `multibackend_graphics_abstraction_rules.md`  
  Rules for layering a portable abstraction over multiple GPU backends.

- `cpp_api_abi_rules.md`  
  Public API/ABI constraints relevant for any library; especially important for GPU public surfaces.

---

## 3. Suggested directory layout (recommended)

```text
/_blueprint/
  _knowledge/
    blueprint_schema.md
    prompt meta rules v1 1.md
    implementation_checklist_schema.md
    decision_log_template.md
    cr_template.md
    walkthrough_template.md
    repo_layout_and_targets.md
    ci_reference.md
    cmake_playbook.md
    dependency_policy.md
    testing_strategy.md
    code_style_and_tooling.md
    error_handling_playbook.md
    logging_observability_policy_new.md
    temp_dbg_policy.md
    performance_benchmark_harness.md
    cpp_api_abi_rules.md
    GPU_APIs_rules.md
    multibackend_graphics_abstraction_rules.md
    knowledge_index.md
  rules/
    (optional) symlinks or duplicated policy subsets used directly by validator/CI
```

---

## 4. Validator integration notes (recommended)

Your composer/validator (COMP-01) should:
- load `blueprint_schema.md` invariants (MD header, no `N/A`, inheritance exactness),
- load CI gate requirements from `ci_reference.md`,
- enforce TEMP debug scanning per `temp_dbg_policy.md`,
- validate checklist YAML schema per `implementation_checklist_schema.md`,
- enforce dependency pinning rules per `dependency_policy.md` (at least “no moving tags”),
- enforce topic-conditional requirements:
  - security/network/persistence: negative+fuzz gates
  - gpu: backend selection DEC + basic backend tests/gates

---

## 5. Change control

All changes to any file listed here MUST be done via CR:
- include SemVer impact rationale
- update validator/gates if schema or invariants change
- provide migration notes

