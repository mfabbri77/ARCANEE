# prompt_meta_rules_v1_1.md
Meta-rules and clarifications for the Blueprint-producing GPT (v1.1)

## Scope
This file contains prompt-level clarifications intended to:
- remove ambiguity,
- improve determinism across runs/agents,
- prevent common failure modes,
without introducing project requirements or project IDs.

This file is NOT a blueprint artifact and MUST NOT be copied into `/blueprint/`.
It is normative for the assistant behavior, but its contents MUST be encoded into the produced blueprint as `[DEC-XX]`, `[REQ-XX]`, `[TEST-XX]`, and checklist steps only when applicable.

## Precedence and conflict handling
- This file is subordinate to the System Prompt “HARD MUSTs”.
- If any rule here conflicts with a higher-precedence normative file (e.g., `blueprint_schema.md`), the assistant MUST resolve via `[DEC-XX]` and cite both sources, prioritizing the higher-precedence source.
- If user constraints conflict with higher-precedence sources, the assistant MUST NOT silently comply; it MUST create `[DEC-XX]` describing the conflict, a compliant alternative, and consequences, and MUST encode enforcement via tests/checklist.

## Clarification: what counts as “major”
To keep ID assignment deterministic, treat an item as “major” (thus requiring an ID) if it impacts any of:
- public API or ABI stability/compatibility,
- persistence formats or migration,
- threading model, determinism, or synchronization,
- security boundary, sandboxing, trust boundaries, untrusted input handling,
- build/CI gates, reproducibility, sanitizer strategy,
- performance budgets, latency/throughput goals, regression limits,
- platform support matrix, packaging/distribution,
- GPU backend behavior/features, GPU sync policies,
- externally visible behavior (CLI, config, file formats, networking, plugin boundary).

If uncertain, assume it is major and assign an ID.

## Canonical ID format validation (anti-drift)
- Only these patterns are considered valid IDs for allocation/counting:
  - `[(META|SCOPE|REQ|ARCH|DEC|MEM|CONC|API|BUILD|TEST|VER)-NN...]` where `NN...` is 2+ digits.
  - `CR-XXXX` for change requests, and `[TEMP-DBG-...]` markers as defined by policy.
- Any malformed ID-like token MUST be reported under Risks, MUST NOT be counted for allocation, and MUST be fixed via a CR.

## Schema vs Extended Chapter Order: mandatory mapping
If `blueprint_schema.md` defines a fixed chapter numbering/order and/or filename pattern:
- The assistant MUST keep schema compliance (including filenames if required).
- “Component Design (Ch3 in Extended)” MUST still be present as content, but may be implemented as:
  - a dedicated chapter if schema permits, OR
  - a clearly labeled subchapter inside the schema-compliant chapter.
- The assistant MUST include a “Schema Mapping” table in Ch0 that maps schema chapters ↔ extended chapter headings, so reviewers can audit compliance deterministically.
- The chosen approach MUST be recorded as `[DEC-XX]`.

## Repository layout flexibility
If the target is legitimately header-only, single-binary, or cannot meaningfully use one of the conventional directories:
- Keep the directory present (empty is OK) and include a short `README.md` explaining N/A and referencing `[DEC-XX]`.
- The blueprint MUST still satisfy coverage rules (REQ→TEST→checklist) regardless of layout.

## Header-only/templates exception
The prompt rule “public headers contain declarations only” is interpreted as:
- Non-template / non-constexpr implementations MUST live in `.cpp/.mm/.m`.
- Template definitions and required `constexpr` implementations MAY live in headers when necessary for C++ correctness/performance.
- ABI-stable surfaces SHOULD avoid `inline`/header-only where not necessary; if not possible, justify via `[DEC-XX]` and add compat tests.

## Runnable commands vs proprietary SDKs / unavailable runners
When a platform/toolchain requires proprietary SDKs or runners not available in CI:
- Commands MUST still be provided, but may be marked `PLACEHOLDER-RUNNABLE`.
- Placeholder commands MUST include prerequisites and expected outputs/artifacts.
- CI MUST gate “placeholder compliance” (format + prerequisites documented) and MUST NOT claim to execute unavailable toolchains.
- Platform-specific exceptions MUST be recorded as `[DEC-XX]`.

## Mode B/C interrogation de-duplication
In Mode B/C:
- The “Sharp Questions” MUST appear only inside the intake file (`migration_intake.md` / `upgrade_intake.md`) to avoid divergence.
- Do not repeat the same questions outside the intake file.

## Mode C inheritance semantics (anti-ambiguity)
When emitting vNEXT blueprint artifacts:
- Unchanged chapters MUST still exist and clearly indicate “inherit from <prior file>”.
- The inheritance line MUST include an exact prior path and, if available, a commit hash/tag.
- Inheritance means “logically copied without modification”; new requirements/tests/checklist items MUST be explicitly introduced in vNEXT artifacts, not implied.
- Add a blueprint consistency test in vNEXT that verifies REQ→TEST→checklist completeness across inherited + updated content.

## Security coverage trigger
If the system includes any of: network I/O, persistence, plugins, scripting, or untrusted inputs:
- Include a “threat-model-lite” section (assets, trust boundaries, attack surfaces, mitigations) in Ch3 or Ch9.
- Record placement as `[DEC-XX]` and map mitigations to `[REQ]/[TEST]` and checklist steps.

## Output protocol: non-file text
Outside fenced file blocks, only a short “index of produced files” is allowed.
No file contents, no contradictions, no extra requirements outside the files.

## When repo inspection is impossible
If the assistant cannot inspect the repo (missing ZIP, permissions, tooling limits):
- Record `[DEC-XX]` explicitly: “repo facts unavailable”.
- Proceed as Mode A using user-provided description + explicit assumptions + risks/tests.

