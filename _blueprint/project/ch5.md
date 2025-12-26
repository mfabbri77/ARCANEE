<!-- _blueprint/project/ch5.md -->

# Blueprint v0.3 — Chapter 5: Data Layout, Memory, Hot Path

## [ARCH-06] Chapter Intent
This chapter specifies memory/layout choices and hot-path considerations for the v0.3 `config/` system, unified theming, Tree-sitter highlighting, and keybindings. It is an **upgrade delta**; all v0.2 data/memory policies remain unless explicitly superseded.

## Existing Memory/Perf Policy
inherit from blueprint_v0.2_ch5.md

---

## [DEC-70] Canonical Color Representation
**Decision**
- Represent all colors internally as **packed 32-bit RGBA** (`0xRRGGBBAA`) in host endianness as an integer value.
- Parse TOML hex strings in the forms:
  - `#RRGGBB` (alpha implied `FF`)
  - `#RRGGBBAA`
- Reject anything else (including named colors) as invalid value → field fallback + diagnostic. [REQ-91], [REQ-97]

**Rationale**
- Packed integer minimizes memory, improves cache locality, and avoids float conversion churn during highlight rendering and ImGui style updates.
- Deterministic canonical format simplifies golden tests. [TEST-40]

**Alternatives**
- Store float4 RGBA: increases memory and parse/convert overhead.
- Store ImGui native `ImVec4`: ties ThemeSystem to ImGui and complicates editor palette reuse.

**Consequences**
- Conversion to ImGui float colors happens at apply-time only (not at parse-time), with deterministic conversion rules.

---

## [ARCH-30] ConfigSnapshot Memory Model (v0.3)
### Snapshot structure
- `ConfigSnapshot` is immutable and shared by `std::shared_ptr<const ConfigSnapshot>` (or equivalent). [API-17]
- Published to main thread only; consumers keep a shared pointer until replaced. [ARCH-12]

### Last-known-good retention
- `ConfigSystem` stores:
  - `last_good` snapshot pointer
  - `current` snapshot pointer
- If parse fails, `current` remains `last_good`. [REQ-91]

### Apply-time allocation policy
- Snapshot build (parsing/validation) may allocate freely on the worker thread.
- Main-thread apply must be **bounded** and avoid unbounded per-frame allocations:
  - Theme apply: O(#tokens + #imgui_colors)
  - Keymap apply: O(#bindings)
  - Font apply: may rebuild atlases/resources, but only on config save and with debouncing. [REQ-91]

---

## [ARCH-31] Theme Data Layout (Editor + Syntax + ImGui)
### EditorPalette
- Store a compact `EditorPalette` of packed RGBA uint32 values (background/foreground/caret/selection/etc.). [REQ-93], [DEC-70]

### SyntaxPalette
- Store as `std::vector<uint32_t> token_rgba` indexed by `SyntaxToken` enum ordinal. [API-17]
  - This makes highlight lookup a single bounds-checked array access.
  - Unknown Tree-sitter captures map to a stable fallback color (typically editor foreground, optional dim). [REQ-94]

### SchemeRegistry
- In-memory scheme registry uses:
  - `unordered_map<string, Scheme>` for lookup by id
  - but **tests and serialization** must canonicalize ordering by sorting scheme ids to avoid hash iteration nondeterminism. [TEST-40]
- Memory note: scheme count is small (O(10–50)); overhead is acceptable.

### ImGui style colors
- Store derived ImGui color table as a vector indexed by `ImGuiCol_*`, packed RGBA.
- Conversion to ImGui float colors is performed at apply-time.
- Derived mapping uses deterministic transforms and is covered by golden tests. [DEC-69], [TEST-40]

---

## [ARCH-32] Tree-sitter Highlighting Hot Path
### Runtime highlight lookup
- The hot path is **capture→token→color**:
  1. capture name (string_view) is mapped to `SyntaxToken` via a stable table (prefer perfect-hash or sorted array + binary search; avoid unordered_map on the hot path). [REQ-94]
  2. `SyntaxToken` ordinal indexes `SyntaxPalette.token_rgba`
- The result is packed RGBA used by the renderer.

### Allocation constraints
- No per-token heap allocations during paint.
- Highlight results for a buffer may be stored in per-buffer vectors and updated incrementally (Tree-sitter incremental parsing); memory reuse via `std::vector::clear()` without shrink.

---

## [ARCH-33] Keymap Data Layout
### Normalized chord representation
- Parse string chords once at load-time into a compact normalized representation (`uint32 packed` or equivalent), platform-normalized. [REQ-96]
- Runtime key dispatch uses:
  - `unordered_map<uint32, action_id>` (or sorted vector + binary search) for chord→action
  - action→chords for UI display/help overlays

### Determinism vs performance
- Conflict resolution is order-dependent by file parse order (last wins). [REQ-96]
- Any user-facing iteration (show bindings, export) must sort action ids/chords deterministically to avoid nondeterministic ordering.

---

## [ARCH-34] Fonts & Resource Rebuild Safety
### Memory/resource strategy
- Font rebuild is the most “heavy” apply operation; it must:
  - build new atlas/resources separately
  - swap to new resources only on success
  - retain old resources on failure (“last-known-good”). [REQ-92], [TEST-42]

### Backend enumeration
- Platform backends may allocate/enumerate sizable font lists; do this only on config changes (not per frame). [DEC-65]

---

## [ARCH-35] Validation/Parsing Memory Notes
- TOML parsing uses `toml++` (DOM parse).
- Parsing is off-thread; main-thread work should not parse TOML.
- Unknown keys are ignored; do not store them in the snapshot (except where required for forward compatibility in **other** systems). Config files are not round-tripped by the IDE in v0.3; preservation is not required here. [REQ-97]

---

## [TEST-44] Color Parsing and Canonicalization
- Given valid strings `#RRGGBB` and `#RRGGBBAA`, parser produces packed RGBA `0xRRGGBBAA` with implied alpha for 6-hex form.
- Invalid forms emit a warning diagnostic with key path and fallback to default color for that field. [REQ-91], [REQ-97]
- Golden test asserts deterministic packed output and deterministic derived ImGui mapping inputs. [DEC-70], [TEST-40]

