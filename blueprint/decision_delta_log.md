# Blueprint v0.3 Decision Delta Log

This file documents design decisions made for v0.3 that supplement or modify the baseline v0.2 decisions.

---

## [DEC-62] Settings/Keybindings Model Replacement
- **Decision**: v0.3 uses `./config/*.toml` and replaces the v0.2 blueprint's layered user/project settings model.
- **Rationale**: Simpler single-location config reduces complexity; users edit config directly.
- **Date**: 2025-12-26

## [DEC-63] GUI Theme Mapping (MVP)
- **Decision**: ImGui style is derived deterministically from scheme base colors for MVP; explicit GUI overrides are optional via `[scheme.gui]`.
- **Rationale**: Reduces config surface while allowing power users to customize.
- **Date**: 2025-12-26

## [DEC-64] TOML Parser Selection
- **Decision**: Use `toml++` for config parsing; Tree-sitter TOML is for highlighting only.
- **Rationale**: `toml++` provides proper DOM access; Tree-sitter is for syntax highlighting.
- **Date**: 2025-12-26

## [DEC-65] System Font Discovery Backends
- **Decision**: Use platform-native discovery (DirectWrite/CoreText/fontconfig); degrade gracefully on failure.
- **Rationale**: Avoids bundling fonts; uses system fonts for consistency with user's environment.
- **Date**: 2025-12-26

## [DEC-66] Explorer Root Modes
- **Decision**: File explorer supports "Project Root" and "Config Root" modes without changing project root.
- **Rationale**: Allows config folder browsing via menu without disrupting project navigation.
- **Date**: 2025-12-26

## [DEC-67] Config Root Resolution
- **Decision**: Primary: `cwd/config/`. Fallback: `exe_dir/config` with warning diagnostic.
- **Rationale**: Supports both development and installed deployment scenarios.
- **Date**: 2025-12-26

## [DEC-68] API Surface Classification
- **Decision**: All config/theme/keymap APIs are internal (not exported as stable SDK ABI).
- **Rationale**: Allows rapid iteration; plugin boundary requires dedicated stabilization CR.
- **Date**: 2025-12-26

## [DEC-69] Derived ImGui Style Mapping
- **Decision**: Deterministic transforms from editor colors: WindowBg from background, Text from foreground, accents from caret/selection.
- **Rationale**: Ensures visual consistency between editor and IDE chrome.
- **Date**: 2025-12-26

## [DEC-70] Canonical Color Representation
- **Decision**: All colors stored as packed 32-bit RGBA (`0xRRGGBBAA`); parse `#RRGGBB` and `#RRGGBBAA` forms only.
- **Rationale**: Minimizes memory, improves cache locality, simplifies golden tests.
- **Date**: 2025-12-26
