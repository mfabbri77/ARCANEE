// ARCANEE Theme System - [API-20]
// Unified theming for editor, syntax, and ImGui.
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#pragma once
#include "ConfigSnapshot.h"
#include <vector>

namespace arcanee::ide::config {

// -----------------------------------------------------------------------------
// ImGui style colors output
// -----------------------------------------------------------------------------
struct ImGuiStyleOut {
  std::vector<uint32_t> imgui_col_rgba; // Indexed by ImGuiCol_*
};

// -----------------------------------------------------------------------------
// ThemeSystem - unified theming [REQ-93], [REQ-94]
// -----------------------------------------------------------------------------
class ThemeSystem {
public:
  ThemeSystem();
  ~ThemeSystem();

  // Apply theme from config snapshot
  void ApplyFromSnapshot(const ConfigSnapshot &snapshot);

  // Accessors
  const EditorPalette &Editor() const { return m_editor; }
  const SyntaxPalette &Syntax() const { return m_syntax; }
  const ImGuiStyleOut &ImGuiStyle() const { return m_imgui; }

  // Get color for a syntax token [REQ-94]
  uint32_t ColorForToken(SyntaxToken token) const;

  // Map Tree-sitter capture name to SyntaxToken
  // Returns true if recognized, false if unknown (use fallback)
  static bool CaptureToToken(const std::string &capture_name,
                             SyntaxToken &out_token);

  // Apply ImGui theme (call from main thread after rendering context is ready)
  void ApplyImGuiTheme();

private:
  void
  DeriveImGuiColors(const EditorPalette &palette,
                    const std::unordered_map<std::string, uint32_t> &overrides);

  EditorPalette m_editor;
  SyntaxPalette m_syntax;
  ImGuiStyleOut m_imgui;
  std::string m_currentSchemeId;
};

} // namespace arcanee::ide::config
