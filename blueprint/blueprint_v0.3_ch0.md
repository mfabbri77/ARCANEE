# Blueprint v0.3 — Chapter 0: Metadata

## [META-01] Version / Date / Repo
- **Repo:** ARCANEE
- **Blueprint version:** v0.3 (target app release: **v0.3.0**)
- **Date:** 2025-12-26
- **Upgrade mode:** Mode C (upgrade)
- **Primary change vehicle:** [CR-0044]
- **Previous blueprint baseline:** `blueprint_v0.2_ch0..ch9.md`

## [META-02] Glossary
inherit from blueprint_v0.2_ch0.md

## [META-03] Assumptions
- **Config location:** repo-root `./config/` is the primary config directory. [DEC-67]
- **Settings model:** v0.3 uses `./config/*.toml` and **replaces** the v0.2 blueprint’s layered user/project settings paths. (No code migration required if those paths are not implemented.) [DEC-62]
- **Hot-apply trigger:** config reload is triggered **only** when `config/*.toml` files are saved **inside the IDE** (no external filesystem watcher). [REQ-91]
- **Encoding:** TOML files are UTF-8 by definition; invalid encoding is rejected at IO boundary and reported via diagnostics; no encoding option exists. [REQ-98]
- **Fonts:** fonts are sourced exclusively from system-installed fonts; no bundled font files; no font-path loading accepted. [REQ-92], [DEC-65]
- **Unified theming:** a single active scheme drives editor UI colors, Tree-sitter token colors, and ImGui style colors. [REQ-93]
- **GUI theme mapping:** ImGui style is derived deterministically from scheme base colors for MVP; explicit GUI overrides are optional. [DEC-63]
- **Tree-sitter coverage:** `.nut` (Squirrel) and `.toml` buffers use Tree-sitter highlighting with a stable capture→token mapping owned by the IDE. [REQ-94]
- **Non-fatal config:** invalid fields fall back per-field to safe defaults; app remains usable; Problems diagnostics are emitted. [REQ-91], [REQ-97]

## [META-04] Risks / Open Questions
- **Working-directory ambiguity:** if the app is launched with a non-repo working directory, `./config` may not resolve as intended; mitigated by `exe_dir/config` compatibility fallback + diagnostic. [DEC-67]
- **Platform font backends:** DirectWrite/CoreText/fontconfig integration may add platform/link complexity; must degrade gracefully and keep last-known-good fonts if discovery fails. [DEC-65], [REQ-92]
- **Theme correctness drift:** Tree-sitter capture sets vary per grammar; mapping stability must be enforced by tests to avoid regressions when updating grammars/queries. [REQ-94], [TEST-40]
- **Hot-apply safety:** font atlas rebuild and theme application must be main-thread safe and not tear UI; failure must preserve last-known-good snapshot. [ARCH-12], [API-17], [TEST-42]

## [META-05] Required `/blueprint` files for v0.3
- `/blueprint/blueprint_v0.3_ch0.md`
- `/blueprint/blueprint_v0.3_ch1.md`
- `/blueprint/blueprint_v0.3_ch2.md`
- `/blueprint/blueprint_v0.3_ch3.md`
- `/blueprint/blueprint_v0.3_ch4.md`
- `/blueprint/blueprint_v0.3_ch5.md`
- `/blueprint/blueprint_v0.3_ch6.md`
- `/blueprint/blueprint_v0.3_ch7.md`
- `/blueprint/blueprint_v0.3_ch8.md`
- `/blueprint/blueprint_v0.3_ch9.md`
- `/blueprint/decision_log.md` (append-only; include new [DEC-62..67])
- `/blueprint/decision_delta_log.md` (v0.3 deltas only; new [DEC] entries since v0.2)
- `/blueprint/walkthrough.md` (v0.3 steps; includes [REQ-90..98])
- `/blueprint/implementation_checklist.yaml` (v0.3 checklist; each [REQ-90..98] referenced)
- `/cr/CR-0044.md`

