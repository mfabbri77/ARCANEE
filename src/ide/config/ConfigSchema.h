// ARCANEE Config Schema - TOML Parsing Helpers
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#pragma once
#include "ConfigSnapshot.h"
#include <functional>
#include <string>
#include <vector>

namespace arcanee::ide::config {

// Forward declaration
struct ConfigSnapshot;

// -----------------------------------------------------------------------------
// ConfigSchema: TOML parsing and validation utilities
// Implements per-field fallback with diagnostics [REQ-91], [REQ-97]
// -----------------------------------------------------------------------------
class ConfigSchema {
public:
  using DiagnosticCallback = std::function<void(const ConfigDiagnostic &)>;

  // Set callback for emitting diagnostics during parsing
  void SetDiagnosticCallback(DiagnosticCallback cb) {
    m_diagCallback = std::move(cb);
  }

  // Parse color-schemes.toml into SchemeRegistry
  bool ParseColorSchemes(const std::string &toml_content,
                         const std::string &file_path,
                         SchemeRegistry &out_registry);

  // Parse editor.toml into EditorConfig
  bool ParseEditorConfig(const std::string &toml_content,
                         const std::string &file_path, EditorConfig &out_config,
                         std::string &out_active_scheme_id);

  // Parse gui.toml into GuiConfig
  bool ParseGuiConfig(const std::string &toml_content,
                      const std::string &file_path, GuiConfig &out_config);

  // Parse keys.toml into KeysConfig
  bool ParseKeysConfig(const std::string &toml_content,
                       const std::string &file_path, KeysConfig &out_config);

  // Utility: Parse hex color string to packed RGBA [DEC-70]
  // Returns true if valid, false otherwise (and default_color is used)
  static bool ParseHexColor(const std::string &hex, uint32_t &out_rgba);

  // Utility: Parse FontWeight from string
  static FontWeight ParseFontWeight(const std::string &str);

  // Utility: Parse FontStyle from string
  static FontStyle ParseFontStyle(const std::string &str);

private:
  DiagnosticCallback m_diagCallback;

  void EmitDiagnostic(const std::string &file, int line, int col, Severity sev,
                      const std::string &msg, const std::string &key_path);

  void EmitWarning(const std::string &file, const std::string &msg,
                   const std::string &key_path);

  void EmitError(const std::string &file, int line, int col,
                 const std::string &msg);
};

} // namespace arcanee::ide::config
