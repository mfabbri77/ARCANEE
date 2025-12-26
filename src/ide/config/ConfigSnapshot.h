// ARCANEE Config Snapshot - [API-17]
// Immutable configuration snapshot passed to subsystems for deterministic
// apply. Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace arcanee::ide::config {

// -----------------------------------------------------------------------------
// Severity for diagnostics [API-18]
// -----------------------------------------------------------------------------
enum class Severity : uint8_t { Info, Warning, Error };

// -----------------------------------------------------------------------------
// Stable IDE-owned token set for syntax highlighting [REQ-94]
// Extend only with SemVer + migration notes [VER-18]
// -----------------------------------------------------------------------------
enum class SyntaxToken : uint16_t {
  Comment = 0,
  String,
  Number,
  Keyword,
  Type,
  Function,
  Variable,
  Operator,
  Error,
  Count // Sentinel for array sizing
};

// -----------------------------------------------------------------------------
// Editor UI palette keys [REQ-93]
// -----------------------------------------------------------------------------
struct EditorPalette {
  uint32_t background_rgba = 0x1F2430FF;
  uint32_t foreground_rgba = 0xCBCCC6FF;
  uint32_t caret_rgba = 0xFFCC66FF;
  uint32_t selection_rgba = 0x33415EFF;
  uint32_t line_highlight_rgba = 0x232A3AFF;
  uint32_t gutter_background_rgba = 0x1F2430FF;
  uint32_t gutter_foreground_rgba = 0x707A8CFF;
};

// -----------------------------------------------------------------------------
// Syntax token palette (indexed by SyntaxToken ordinal)
// -----------------------------------------------------------------------------
struct SyntaxPalette {
  std::vector<uint32_t> token_rgba; // Size = SyntaxToken::Count

  SyntaxPalette()
      : token_rgba(static_cast<size_t>(SyntaxToken::Count), 0xCBCCC6FF) {}

  uint32_t GetColor(SyntaxToken t) const {
    auto idx = static_cast<size_t>(t);
    return idx < token_rgba.size() ? token_rgba[idx] : 0xCBCCC6FF;
  }
};

// -----------------------------------------------------------------------------
// Font specification [REQ-92]
// -----------------------------------------------------------------------------
enum class FontWeight : uint8_t { Light, Regular, Medium, SemiBold, Bold };
enum class FontStyle : uint8_t { Normal, Italic };

struct FontSpec {
  std::string family = "monospace";
  float size_px = 14.0f;
  FontWeight weight = FontWeight::Regular;
  FontStyle style = FontStyle::Normal;
  float line_height = 1.2f; // multiplier
  bool ligatures = false;
};

// -----------------------------------------------------------------------------
// Editor configuration
// -----------------------------------------------------------------------------
struct EditorConfig {
  FontSpec font;

  enum class IndentType : uint8_t { Spaces, Tabs };
  IndentType indent_type = IndentType::Spaces;
  int indent_width = 2;
  int tab_width = 2;
  bool indent_detect = false;

  bool line_numbers = true;
  bool highlight_current_line = true;

  bool cursor_blink = true;
  int cursor_blink_rate_ms = 530;
};

// -----------------------------------------------------------------------------
// GUI configuration
// -----------------------------------------------------------------------------
struct GuiConfig {
  FontSpec ui_font;

  float title_bar_height_px = 0.0f;
  float menu_bar_height_px = 0.0f;
  float dock_padding_px = 6.0f;
  float window_rounding_px = 6.0f;
  float scrollbar_size_px = 14.0f;

  bool remember_layout = true;
  bool show_fps = false;
  float dpi_scale = 0.0f; // 0 => auto
};

// -----------------------------------------------------------------------------
// Normalized chord representation [REQ-96]
// -----------------------------------------------------------------------------
struct Chord {
  uint32_t packed = 0; // Platform-normalized key chord

  bool operator==(const Chord &other) const { return packed == other.packed; }
};

// Hash for Chord in unordered_map
struct ChordHash {
  size_t operator()(const Chord &c) const {
    return std::hash<uint32_t>{}(c.packed);
  }
};

// -----------------------------------------------------------------------------
// Keybindings configuration
// -----------------------------------------------------------------------------
struct KeysConfig {
  std::unordered_map<std::string, std::vector<Chord>> action_to_chords;
  std::unordered_map<uint32_t, std::string>
      chord_to_action; // packed -> action id
};

// -----------------------------------------------------------------------------
// Color scheme definition
// -----------------------------------------------------------------------------
struct Scheme {
  std::string id;
  std::string name;
  std::string variant; // "light" / "dark"
  EditorPalette editor;
  SyntaxPalette syntax;
  std::unordered_map<std::string, uint32_t> gui_overrides; // Optional [DEC-63]
};

// -----------------------------------------------------------------------------
// Scheme registry
// -----------------------------------------------------------------------------
struct SchemeRegistry {
  std::unordered_map<std::string, Scheme> schemes_by_id;

  const Scheme *Find(const std::string &id) const {
    auto it = schemes_by_id.find(id);
    return it != schemes_by_id.end() ? &it->second : nullptr;
  }
};

// -----------------------------------------------------------------------------
// Paths configuration
// -----------------------------------------------------------------------------
struct Paths {
  std::string config_root;
};

// -----------------------------------------------------------------------------
// ConfigSnapshot - immutable configuration snapshot [API-17]
// -----------------------------------------------------------------------------
struct ConfigSnapshot {
  Paths paths;
  SchemeRegistry registry;
  std::string active_scheme_id = "ayu-mirage";
  EditorConfig editor;
  GuiConfig gui;
  KeysConfig keys;
  uint64_t version = 0; // Monotonically increasing for change detection
};

using ConfigSnapshotPtr = std::shared_ptr<const ConfigSnapshot>;

// -----------------------------------------------------------------------------
// Diagnostic structure for Problems integration [API-18], [REQ-97]
// -----------------------------------------------------------------------------
struct SourceRange {
  int line = 0;   // 1-based; 0 means unknown
  int column = 0; // 1-based; 0 means unknown
  int end_line = 0;
  int end_column = 0;
};

struct ConfigDiagnostic {
  std::string file;  // e.g., "config/editor.toml"
  SourceRange range; // Best-effort
  Severity severity = Severity::Warning;
  std::string message;  // Concise reason + fallback used
  std::string key_path; // e.g., "font.size_px"
};

} // namespace arcanee::ide::config
