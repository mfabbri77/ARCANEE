<!-- _blueprint/project/ch4.md -->

# Blueprint v0.3 — Chapter 4: Interfaces, API/ABI, Error Model

## [ARCH-04] Chapter Intent
This chapter defines the new internal APIs introduced by [CR-0044] for config, theming, keybindings, Tree-sitter highlighting integration points, and system font selection. Public ABI stability constraints inherit from v0.2.

## [DEC-68] API Surface Classification
- All APIs in this chapter are **internal** (not exported as a stable SDK ABI).
- Public “engine” ABI rules remain as defined in v0.2; this change does not introduce new exported symbols.
- If any of these modules becomes a plugin boundary later, a dedicated ABI stabilization CR is required.

---

## [ARCH-05] Existing API/ABI Rules
inherit from blueprint_v0.2_ch4.md

---

## [API-17] ConfigSnapshot Contract
### Purpose
Immutable snapshot passed to subsystems to apply configuration deterministically and safely. [ARCH-12]

### Ownership / Lifetime
- Owned by `ConfigSystem` as `std::shared_ptr<const ConfigSnapshot>` or equivalent.
- Consumers store a copy of the shared pointer (never mutate).

### Threading
- Produced off-thread; published to main thread via apply queue. [ARCH-12]
- Consumers apply on main thread.

### Error model
- Snapshot build never throws across boundaries; parse/validate errors are converted into diagnostics and fallback values in the snapshot. [REQ-91], [REQ-97]

### Header sketch (declarations only)
```cpp
// [API-17]
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace arc::ide::config {

enum class Severity : uint8_t { Info, Warning, Error };

// Stable IDE-owned token set used by syntax highlighting. [REQ-94]
enum class SyntaxToken : uint16_t {
  Comment, String, Number, Keyword, Type, Function, Variable, Operator, Error,
  // v0.3 stable set; extend only with SemVer + migration notes. [VER-18]
};

// Editor UI palette keys (non-exhaustive; stable as a set, values come from scheme). [REQ-93]
struct EditorPalette {
  uint32_t background_rgba = 0;
  uint32_t foreground_rgba = 0;
  uint32_t caret_rgba = 0;
  uint32_t selection_rgba = 0;
  uint32_t line_highlight_rgba = 0;
  uint32_t gutter_background_rgba = 0;
  uint32_t gutter_foreground_rgba = 0;
};

struct SyntaxPalette {
  // index by SyntaxToken as (uint16_t) value
  std::vector<uint32_t> token_rgba;
};

enum class FontWeight : uint8_t { Light, Regular, Medium, SemiBold, Bold };
enum class FontStyle  : uint8_t { Normal, Italic };

struct FontSpec {
  std::string family;
  float size_px = 14.0f;
  FontWeight weight = FontWeight::Regular;
  FontStyle style = FontStyle::Normal;
  float line_height = 1.0f; // multiplier
  bool ligatures = false;
  // antialiasing/hinting are hints; ignored if unsupported. [REQ-92]
};

struct EditorConfig {
  FontSpec font;
  // indentation
  enum class IndentType : uint8_t { Spaces, Tabs };
  IndentType indent_type = IndentType::Spaces;
  int indent_width = 2;
  int tab_width = 2;
  bool indent_detect = false;
  // behavior (representative; full set in ConfigSchema)
  bool line_numbers = true;
  bool highlight_current_line = true;
};

struct GuiConfig {
  FontSpec ui_font;
  // metrics
  float title_bar_height_px = 0.0f;
  float menu_bar_height_px = 0.0f;
  float dock_padding_px = 0.0f;
  float window_rounding_px = 0.0f;
  float scrollbar_size_px = 0.0f;
  // behavior
  bool remember_layout = true;
  bool show_fps = false;
  float dpi_scale = 0.0f; // 0 => auto
};

struct Chord {
  // normalized platform chord; exact representation defined by Keymap. [REQ-96]
  uint32_t packed = 0;
};

struct KeysConfig {
  std::unordered_map<std::string, std::vector<Chord>> action_to_chords;
  std::unordered_map<uint32_t, std::string> chord_to_action; // packed -> action id
};

struct Scheme {
  std::string id;
  std::string name;
  std::string variant; // "light"/"dark"/etc
  EditorPalette editor;
  SyntaxPalette syntax;
  // GUI overrides optional; if absent derive deterministically. [DEC-63]
  // Stored as key->rgba; mapping to ImGuiCol_* happens in ThemeSystem.
  std::unordered_map<std::string, uint32_t> gui_overrides;
};

struct SchemeRegistry {
  std::unordered_map<std::string, Scheme> schemes_by_id;
};

struct Paths {
  std::string config_root;
};

struct ConfigSnapshot {
  Paths paths;
  SchemeRegistry registry;
  std::string active_scheme_id;
  EditorConfig editor;
  GuiConfig gui;
  KeysConfig keys;
  uint64_t version = 0;
};

using ConfigSnapshotPtr = std::shared_ptr<const ConfigSnapshot>;

} // namespace arc::ide::config
````

---

## [API-18] Problems Diagnostics Emission (Config)

### Purpose

Emit structured diagnostics to the Problems window for config parsing/validation issues. [REQ-97], [REQ-91]

### Ownership / Lifetime

* Diagnostics objects are value types; owned by Problems subsystem.

### Threading

* Diagnostics may be created off-thread but must be published to the UI/main thread consistent with existing Problems subsystem rules. [ARCH-12]

### Error model

* Never throws; must be safe even during partial config failure.

### Header sketch

```cpp
// [API-18]
#pragma once
#include <string>
#include <vector>

namespace arc::ide {

struct SourceRange {
  // 1-based line/column; 0 means "unknown".
  int line = 0;
  int column = 0;
  int end_line = 0;
  int end_column = 0;
};

enum class DiagnosticSeverity : uint8_t { Info, Warning, Error };

struct Diagnostic {
  std::string file;      // e.g., "config/editor.toml" [REQ-97]
  SourceRange range;     // best-effort
  DiagnosticSeverity sev;
  std::string message;   // concise reason + fallback used
  std::string key_path;  // e.g., "font.size_px"
};

class ProblemsSink {
public:
  virtual ~ProblemsSink() = default;
  virtual void ReplaceDiagnosticsForFile(const std::string& file,
                                        const std::vector<Diagnostic>& diags) = 0;
};

} // namespace arc::ide
```

---

## [API-19] ConfigSystem API

### Purpose

Central coordinator for config load/merge/validate + hot-apply on IDE save. [REQ-90], [REQ-91], [REQ-97], [REQ-98], [ARCH-12]

### Ownership

* Singleton-ish owned by `UIShell` (or a top-level IDE `AppServices` container).
* Depends on `ProblemsSink`, `DocumentSystem` save events, and an apply callback.

### Threading / Sync

* Accepts save events on main thread.
* Uses worker thread for parse/validate (no UI calls).
* Publishes `ConfigSnapshotPtr` to main thread through `ApplyQueue`.

### Error model

* Parse errors: retain last-known-good snapshot; emit error diagnostics. [REQ-91]
* Validation errors: per-field fallback; emit warning diagnostics. [REQ-91]
* IO encoding error: emit error diagnostics; retain last-known-good. [REQ-98]

### Header sketch

```cpp
// [API-19]
#pragma once
#include <functional>
#include <string>
#include "ConfigSnapshot.h" // [API-17]
#include "Problems.h"       // [API-18]

namespace arc::ide::config {

struct ConfigSystemInit {
  arc::ide::ProblemsSink* problems = nullptr; // non-owning
  std::function<void(ConfigSnapshotPtr)> apply_on_main; // called on main thread
  std::function<void(std::function<void()>)> post_to_main; // enqueue to main/UI
  std::function<void(std::function<void()>)> post_to_worker; // enqueue to worker
};

class ConfigSystem {
public:
  explicit ConfigSystem(const ConfigSystemInit& init);

  // Called once at startup; loads defaults + config files; may create missing
  // shipped defaults (e.g., color-schemes.toml) in dev mode. [REQ-90]
  void Initialize();

  // Called by DocumentSystem when IDE saves a file. [REQ-91]
  void OnIdeSavedFile(const std::string& absolute_path);

  // Access last published snapshot (thread: main).
  ConfigSnapshotPtr Current() const;

private:
  void DebouncedReload();
};

} // namespace arc::ide::config
```

---

## [API-20] ThemeSystem API

### Purpose

Produce palettes and apply unified theming to editor + Tree-sitter token colors + ImGui style. [REQ-93], [REQ-94], [DEC-63]

### Ownership

* Owned by UIShell/app services.
* Consumes `ConfigSnapshot` only (no TOML access). [API-17]

### Threading

* Apply on main thread.

### Error model

* Missing scheme id: must have already been resolved by ConfigSystem into fallback active scheme id; ThemeSystem assumes snapshot is internally consistent.

### Header sketch

```cpp
// [API-20]
#pragma once
#include <cstdint>
#include <vector>
#include "ConfigSnapshot.h" // [API-17]

namespace arc::ide::config {

struct ImGuiStyleOut {
  // ImGui style colors expressed as RGBA floats or packed; representation chosen in implementation. [DEC-63]
  std::vector<uint32_t> imgui_col_rgba; // index by ImGuiCol_*
};

class ThemeSystem {
public:
  void ApplyFromSnapshot(const ConfigSnapshot& s); // [REQ-93]
  const EditorPalette& Editor() const;
  const SyntaxPalette& Syntax() const;
  const ImGuiStyleOut& ImGui() const;

  // Capture->token mapping entrypoint for ParseService. [REQ-94]
  uint32_t ColorForToken(SyntaxToken t) const;

private:
  EditorPalette editor_;
  SyntaxPalette syntax_;
  ImGuiStyleOut imgui_;
};

} // namespace arc::ide::config
```

---

## [API-21] Keymap API

### Purpose

Parse and apply `config/keys.toml` to build key dispatch tables with deterministic conflict resolution. [REQ-96]

### Ownership

* Owned by UIShell/app services.
* Consumes `ConfigSnapshot` only.

### Threading

* Apply on main thread.
* Key event processing occurs on main thread.

### Error model

* Invalid chord strings are dropped and warned during ConfigSystem validation; Keymap assumes normalized chords in snapshot.

### Header sketch

```cpp
// [API-21]
#pragma once
#include <string>
#include <vector>
#include "ConfigSnapshot.h" // [API-17]

namespace arc::ide::config {

class Keymap {
public:
  void ApplyFromSnapshot(const ConfigSnapshot& s); // [REQ-96]

  // Returns empty if not bound.
  std::string ActionForChord(uint32_t chord_packed) const;

  // For UI display.
  std::vector<Chord> ChordsForAction(const std::string& action_id) const;

private:
  KeysConfig keys_;
};

} // namespace arc::ide::config
```

---

## [API-22] Font Locator API (Platform)

### Purpose

Locate and load system fonts for editor and GUI; rebuild safely; keep last-known-good. [REQ-92], [DEC-65]

### Ownership

* Owned by UIShell/app services.
* Platform-specific implementation selected via compile-time switch.

### Threading

* Font atlas rebuild must occur on main thread (ImGui/editor GPU resources). [ARCH-12]

### Error model

* On failure, keep previous resources and surface diagnostics through Problems/logging. [TEST-42]

### Header sketch

```cpp
// [API-22]
#pragma once
#include <string>
#include "ConfigSnapshot.h" // FontSpec [API-17]

namespace arc::ide::config {

struct FontLoadResult {
  bool ok = false;
  std::string message; // for diagnostics on failure
};

class FontLocator {
public:
  virtual ~FontLocator() = default;

  // Called when config changes or at startup. [REQ-92]
  virtual FontLoadResult ApplyEditorFont(const FontSpec& spec) = 0;
  virtual FontLoadResult ApplyUiFont(const FontSpec& spec) = 0;
};

} // namespace arc::ide::config
```

---

## [API-23] ParseService Integration Points (Tree-sitter)

### Purpose

Allow ParseService to request token colors from ThemeSystem via stable token enum mapping. [REQ-94]

### Decision: Stable mapping layer

* Tree-sitter capture names (`@comment`, `@string`, etc.) map to IDE-owned `SyntaxToken`.
* Unknown captures map to `EditorPalette.foreground_rgba` (optional dimming is a ThemeSystem implementation detail). [REQ-94]

### Header sketch (conceptual)

```cpp
// [API-23]
#pragma once
#include <string_view>
#include "ConfigSnapshot.h" // SyntaxToken [API-17]

namespace arc::ide::config {

// Returns true if capture recognized.
bool CaptureToToken(std::string_view capture_name, SyntaxToken& out);

} // namespace arc::ide::config
```

---

## [DEC-69] Derived ImGui Style Mapping (MVP)

* If `[scheme.gui]` overrides exist, apply them first for known keys (e.g., `window_bg`, `text`, `accent`).
* Otherwise derive from `[scheme.editor]` base colors via deterministic transforms:

  * `WindowBg` from editor.background
  * `Text` from editor.foreground
  * `Header` and `Button` from a lightened/darkened accent derived from caret or selection color
  * Hover/Active states use fixed multipliers.
* Mapping must be deterministic and covered by golden tests. [TEST-40]

---

## [BUILD-18] Build Integration Commands (v0.3 delta)

inherit from blueprint_v0.2_ch7.md

Add to CI/build matrix:

* Ensure `tree-sitter-toml` grammar is built and linked.
* Ensure platform font backends link:

  * Windows: DirectWrite
  * macOS: CoreText
  * Linux: fontconfig (optional; warn if not found and enable fallback path).

---

## [TEST] API Contract Tests

* [TEST-39] exercises ConfigSystem snapshot build + diagnostics emission contract.
* [TEST-40] validates ThemeSystem’s derived mapping determinism and token color lookups.
* [TEST-41] validates Keymap normalization and determinism.
* [TEST-42] validates FontLocator failure fallback behavior.
* [TEST-43] validates Configuration menu hooks into DocumentSystem save events and explorer mode without breaking project root.


