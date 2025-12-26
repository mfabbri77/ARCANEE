// ARCANEE Config Schema - TOML Parsing Implementation
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#include "ConfigSchema.h"
#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

namespace arcanee::ide::config {

// -----------------------------------------------------------------------------
// Utility: Parse hex color string to packed RGBA [DEC-70]
// Supports: #RRGGBB (alpha implied FF) and #RRGGBBAA
// -----------------------------------------------------------------------------
bool ConfigSchema::ParseHexColor(const std::string &hex, uint32_t &out_rgba) {
  if (hex.empty() || hex[0] != '#') {
    return false;
  }

  std::string digits = hex.substr(1);
  if (digits.length() != 6 && digits.length() != 8) {
    return false;
  }

  // Validate all characters are hex digits
  for (char c : digits) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }

  try {
    if (digits.length() == 6) {
      // #RRGGBB -> 0xRRGGBBAA with AA=FF
      uint32_t rgb = std::stoul(digits, nullptr, 16);
      out_rgba = (rgb << 8) | 0xFF;
    } else {
      // #RRGGBBAA -> 0xRRGGBBAA
      out_rgba = std::stoul(digits, nullptr, 16);
    }
    return true;
  } catch (...) {
    return false;
  }
}

// -----------------------------------------------------------------------------
// Utility: Parse FontWeight from string
// -----------------------------------------------------------------------------
FontWeight ConfigSchema::ParseFontWeight(const std::string &str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lower == "light")
    return FontWeight::Light;
  if (lower == "medium")
    return FontWeight::Medium;
  if (lower == "semibold")
    return FontWeight::SemiBold;
  if (lower == "bold")
    return FontWeight::Bold;
  return FontWeight::Regular; // Default
}

// -----------------------------------------------------------------------------
// Utility: Parse FontStyle from string
// -----------------------------------------------------------------------------
FontStyle ConfigSchema::ParseFontStyle(const std::string &str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lower == "italic")
    return FontStyle::Italic;
  return FontStyle::Normal;
}

// -----------------------------------------------------------------------------
// Diagnostic emission helpers
// -----------------------------------------------------------------------------
void ConfigSchema::EmitDiagnostic(const std::string &file, int line, int col,
                                  Severity sev, const std::string &msg,
                                  const std::string &key_path) {
  if (m_diagCallback) {
    ConfigDiagnostic diag;
    diag.file = file;
    diag.range.line = line;
    diag.range.column = col;
    diag.severity = sev;
    diag.message = msg;
    diag.key_path = key_path;
    m_diagCallback(diag);
  }
  // Also log
  if (sev == Severity::Error) {
    spdlog::error("[Config] {}: {} ({})", file, msg, key_path);
  } else if (sev == Severity::Warning) {
    spdlog::warn("[Config] {}: {} ({})", file, msg, key_path);
  }
}

void ConfigSchema::EmitWarning(const std::string &file, const std::string &msg,
                               const std::string &key_path) {
  EmitDiagnostic(file, 0, 0, Severity::Warning, msg, key_path);
}

void ConfigSchema::EmitError(const std::string &file, int line, int col,
                             const std::string &msg) {
  EmitDiagnostic(file, line, col, Severity::Error, msg, "");
}

// -----------------------------------------------------------------------------
// Parse color-schemes.toml
// -----------------------------------------------------------------------------
bool ConfigSchema::ParseColorSchemes(const std::string &toml_content,
                                     const std::string &file_path,
                                     SchemeRegistry &out_registry) {
  toml::table tbl;
  try {
    tbl = toml::parse(toml_content);
  } catch (const toml::parse_error &err) {
    EmitError(file_path, err.source().begin.line, err.source().begin.column,
              std::string("TOML parse error: ") +
                  std::string(err.description()));
    return false;
  }

  // Parse [[scheme]] array
  if (auto schemes = tbl["scheme"].as_array()) {
    for (const auto &scheme_node : *schemes) {
      if (!scheme_node.is_table())
        continue;
      const auto &scheme_tbl = *scheme_node.as_table();

      Scheme scheme;

      // Required: id
      if (auto id = scheme_tbl["id"].value<std::string>()) {
        scheme.id = *id;
      } else {
        EmitWarning(file_path, "Scheme missing 'id', skipping", "scheme.id");
        continue;
      }

      // Optional: name, variant
      if (auto name = scheme_tbl["name"].value<std::string>()) {
        scheme.name = *name;
      } else {
        scheme.name = scheme.id;
      }
      if (auto variant = scheme_tbl["variant"].value<std::string>()) {
        scheme.variant = *variant;
      }

      // Parse [scheme.editor] colors
      if (auto editor = scheme_tbl["editor"].as_table()) {
        auto parseColor = [&](const char *key, uint32_t &target, uint32_t def) {
          if (auto val = (*editor)[key].value<std::string>()) {
            if (!ParseHexColor(*val, target)) {
              EmitWarning(file_path, "Invalid color format, using default",
                          std::string("scheme.editor.") + key);
              target = def;
            }
          } else {
            target = def;
          }
        };

        parseColor("background", scheme.editor.background_rgba, 0x1F2430FF);
        parseColor("foreground", scheme.editor.foreground_rgba, 0xCBCCC6FF);
        parseColor("caret", scheme.editor.caret_rgba, 0xFFCC66FF);
        parseColor("selection", scheme.editor.selection_rgba, 0x33415EFF);
        parseColor("line_highlight", scheme.editor.line_highlight_rgba,
                   0x232A3AFF);
        parseColor("gutter_background", scheme.editor.gutter_background_rgba,
                   0x1F2430FF);
        parseColor("gutter_foreground", scheme.editor.gutter_foreground_rgba,
                   0x707A8CFF);
      }

      // Parse [scheme.syntax] colors
      if (auto syntax = scheme_tbl["syntax"].as_table()) {
        scheme.syntax.token_rgba.resize(
            static_cast<size_t>(SyntaxToken::Count));

        auto parseSyntaxColor = [&](const char *key, SyntaxToken token,
                                    uint32_t def) {
          if (auto val = (*syntax)[key].value<std::string>()) {
            uint32_t color;
            if (ParseHexColor(*val, color)) {
              scheme.syntax.token_rgba[static_cast<size_t>(token)] = color;
            } else {
              EmitWarning(file_path, "Invalid syntax color, using default",
                          std::string("scheme.syntax.") + key);
              scheme.syntax.token_rgba[static_cast<size_t>(token)] = def;
            }
          } else {
            scheme.syntax.token_rgba[static_cast<size_t>(token)] = def;
          }
        };

        parseSyntaxColor("comment", SyntaxToken::Comment, 0x707A8CFF);
        parseSyntaxColor("string", SyntaxToken::String, 0xBAE67EFF);
        parseSyntaxColor("number", SyntaxToken::Number, 0xD4BFFFFF);
        parseSyntaxColor("keyword", SyntaxToken::Keyword, 0xFFAD66FF);
        parseSyntaxColor("type", SyntaxToken::Type, 0x73D0FFFF);
        parseSyntaxColor("function", SyntaxToken::Function, 0xFFD173FF);
        parseSyntaxColor("variable", SyntaxToken::Variable, 0xCBCCC6FF);
        parseSyntaxColor("operator", SyntaxToken::Operator, 0xF29E74FF);
        parseSyntaxColor("error", SyntaxToken::Error, 0xFF6666FF);
      }

      // Parse optional [scheme.gui] overrides
      if (auto gui = scheme_tbl["gui"].as_table()) {
        for (const auto &[key, val] : *gui) {
          if (auto str = val.value<std::string>()) {
            uint32_t color;
            if (ParseHexColor(*str, color)) {
              scheme.gui_overrides[std::string(key.str())] = color;
            }
          }
        }
      }

      out_registry.schemes_by_id[scheme.id] = std::move(scheme);
    }
  }

  // Warn about unknown top-level keys
  for (const auto &[key, val] : tbl) {
    if (key != "scheme") {
      EmitWarning(file_path, "Unknown configuration key",
                  std::string(key.str()));
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// Parse editor.toml
// -----------------------------------------------------------------------------
bool ConfigSchema::ParseEditorConfig(const std::string &toml_content,
                                     const std::string &file_path,
                                     EditorConfig &out_config,
                                     std::string &out_active_scheme_id) {
  toml::table tbl;
  try {
    tbl = toml::parse(toml_content);
  } catch (const toml::parse_error &err) {
    EmitError(file_path, err.source().begin.line, err.source().begin.column,
              std::string("TOML parse error: ") +
                  std::string(err.description()));
    return false;
  }

  // [color_scheme]
  if (auto cs = tbl["color_scheme"].as_table()) {
    if (auto active = (*cs)["active"].value<std::string>()) {
      out_active_scheme_id = *active;
    }
  }

  // [font]
  if (auto font = tbl["font"].as_table()) {
    if (auto family = (*font)["family"].value<std::string>()) {
      // Check for path-like values [REQ-92]
      if (family->find('/') != std::string::npos ||
          family->find('\\') != std::string::npos) {
        EmitWarning(file_path, "Font file paths not allowed, using default",
                    "font.family");
      } else {
        out_config.font.family = *family;
      }
    }
    auto valNode = (*font)["size_px"];
    if (auto d = valNode.value<double>()) {
      out_config.font.size_px = static_cast<float>(*d);
    } else if (auto i = valNode.value<int64_t>()) {
      out_config.font.size_px = static_cast<float>(*i);
    } else if (auto s = valNode.value<std::string>()) {
      try {
        out_config.font.size_px = std::stof(*s);
      } catch (...) {
      }
    }
    if (auto weight = (*font)["weight"].value<std::string>()) {
      out_config.font.weight = ParseFontWeight(*weight);
    }
    if (auto style = (*font)["style"].value<std::string>()) {
      out_config.font.style = ParseFontStyle(*style);
    }
    if (auto lh = (*font)["line_height"].value<double>()) {
      out_config.font.line_height = static_cast<float>(*lh);
    }
    if (auto lig = (*font)["ligatures"].value<bool>()) {
      out_config.font.ligatures = *lig;
    }
  }

  // [indent]
  if (auto indent = tbl["indent"].as_table()) {
    if (auto type = (*indent)["type"].value<std::string>()) {
      out_config.indent_type = (*type == "tabs")
                                   ? EditorConfig::IndentType::Tabs
                                   : EditorConfig::IndentType::Spaces;
    }
    if (auto width = (*indent)["width"].value<int64_t>()) {
      out_config.indent_width = static_cast<int>(*width);
    }
    if (auto tw = (*indent)["tab_width"].value<int64_t>()) {
      out_config.tab_width = static_cast<int>(*tw);
    }
    if (auto detect = (*indent)["detect"].value<bool>()) {
      out_config.indent_detect = *detect;
    }
  }

  // [view]
  if (auto view = tbl["view"].as_table()) {
    if (auto ln = (*view)["line_numbers"].value<bool>()) {
      out_config.line_numbers = *ln;
    }
    if (auto hl = (*view)["highlight_current_line"].value<bool>()) {
      out_config.highlight_current_line = *hl;
    }
  }

  // [cursor]
  if (auto cursor = tbl["cursor"].as_table()) {
    if (auto blink = (*cursor)["blink"].value<bool>()) {
      out_config.cursor_blink = *blink;
    }
    if (auto rate = (*cursor)["blink_rate_ms"].value<int64_t>()) {
      out_config.cursor_blink_rate_ms = static_cast<int>(*rate);
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// Parse gui.toml
// -----------------------------------------------------------------------------
bool ConfigSchema::ParseGuiConfig(const std::string &toml_content,
                                  const std::string &file_path,
                                  GuiConfig &out_config) {
  toml::table tbl;
  try {
    tbl = toml::parse(toml_content);
  } catch (const toml::parse_error &err) {
    EmitError(file_path, err.source().begin.line, err.source().begin.column,
              std::string("TOML parse error: ") +
                  std::string(err.description()));
    return false;
  }

  // [ui_font]
  if (auto font = tbl["ui_font"].as_table()) {
    if (auto family = (*font)["family"].value<std::string>()) {
      if (family->find('/') != std::string::npos ||
          family->find('\\') != std::string::npos) {
        EmitWarning(file_path, "Font file paths not allowed", "ui_font.family");
      } else {
        out_config.ui_font.family = *family;
      }
    }
    if (auto size = (*font)["size_px"].value<double>()) {
      out_config.ui_font.size_px = static_cast<float>(*size);
    }
    if (auto weight = (*font)["weight"].value<std::string>()) {
      out_config.ui_font.weight = ParseFontWeight(*weight);
    }
    if (auto lh = (*font)["line_height"].value<double>()) {
      out_config.ui_font.line_height = static_cast<float>(*lh);
    }
  }

  // [metrics]
  if (auto metrics = tbl["metrics"].as_table()) {
    if (auto v = (*metrics)["title_bar_height_px"].value<double>()) {
      out_config.title_bar_height_px = static_cast<float>(*v);
    }
    if (auto v = (*metrics)["menu_bar_height_px"].value<double>()) {
      out_config.menu_bar_height_px = static_cast<float>(*v);
    }
    if (auto v = (*metrics)["dock_padding_px"].value<double>()) {
      out_config.dock_padding_px = static_cast<float>(*v);
    }
    if (auto v = (*metrics)["window_rounding_px"].value<double>()) {
      out_config.window_rounding_px = static_cast<float>(*v);
    }
    if (auto v = (*metrics)["scrollbar_size_px"].value<double>()) {
      out_config.scrollbar_size_px = static_cast<float>(*v);
    }
  }

  // [behavior]
  if (auto behavior = tbl["behavior"].as_table()) {
    if (auto v = (*behavior)["remember_layout"].value<bool>()) {
      out_config.remember_layout = *v;
    }
    if (auto v = (*behavior)["show_fps"].value<bool>()) {
      out_config.show_fps = *v;
    }
    if (auto v = (*behavior)["dpi_scale"].value<double>()) {
      out_config.dpi_scale = static_cast<float>(*v);
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// Parse keys.toml
// -----------------------------------------------------------------------------
bool ConfigSchema::ParseKeysConfig(const std::string &toml_content,
                                   const std::string &file_path,
                                   KeysConfig &out_config) {
  toml::table tbl;
  try {
    tbl = toml::parse(toml_content);
  } catch (const toml::parse_error &err) {
    EmitError(file_path, err.source().begin.line, err.source().begin.column,
              std::string("TOML parse error: ") +
                  std::string(err.description()));
    return false;
  }

  // [keys] table contains action_id = ["chord1", "chord2", ...]
  if (auto keys = tbl["keys"].as_table()) {
    for (const auto &[action_key, val] : *keys) {
      std::string action_id(action_key.str());

      if (auto arr = val.as_array()) {
        std::vector<Chord> chords;
        for (const auto &chord_val : *arr) {
          if (auto chord_str = chord_val.value<std::string>()) {
            // Chord parsing will be done by Keymap class
            // For now, store as packed hash for placeholder
            Chord c;
            c.packed = std::hash<std::string>{}(*chord_str) & 0xFFFFFFFF;
            chords.push_back(c);
          }
        }
        out_config.action_to_chords[action_id] = std::move(chords);
      }
    }
  }

  return true;
}

} // namespace arcanee::ide::config
