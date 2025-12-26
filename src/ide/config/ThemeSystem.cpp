// ARCANEE Theme System - Implementation
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#include "ThemeSystem.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace arcanee::ide::config {

// -----------------------------------------------------------------------------
// Static capture name to token mapping [REQ-94]
// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, SyntaxToken> kCaptureMap = {
    // Comment captures
    {"comment", SyntaxToken::Comment},
    {"comment.line", SyntaxToken::Comment},
    {"comment.block", SyntaxToken::Comment},

    // String captures
    {"string", SyntaxToken::String},
    {"string.special", SyntaxToken::String},

    // Number captures
    {"number", SyntaxToken::Number},
    {"float", SyntaxToken::Number},
    {"number.float", SyntaxToken::Number},

    // Keyword captures
    {"keyword", SyntaxToken::Keyword},
    {"keyword.control", SyntaxToken::Keyword},
    {"keyword.operator", SyntaxToken::Operator},
    {"keyword.return", SyntaxToken::Keyword},
    {"keyword.function", SyntaxToken::Keyword},

    // Type captures
    {"type", SyntaxToken::Type},
    {"type.builtin", SyntaxToken::Type},

    // Function captures
    {"function", SyntaxToken::Function},
    {"function.builtin", SyntaxToken::Function},
    {"function.call", SyntaxToken::Function},
    {"function.method", SyntaxToken::Function},

    // Variable captures
    {"variable", SyntaxToken::Variable},
    {"variable.builtin", SyntaxToken::Variable},
    {"constant", SyntaxToken::Variable},
    {"constant.builtin", SyntaxToken::Variable},
    {"property", SyntaxToken::Variable},

    // Operator captures
    {"operator", SyntaxToken::Operator},
    {"punctuation", SyntaxToken::Operator},
    {"punctuation.delimiter", SyntaxToken::Operator},
    {"punctuation.bracket", SyntaxToken::Operator},

    // Error captures
    {"error", SyntaxToken::Error},
};

// -----------------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------------
ThemeSystem::ThemeSystem() {
  // Initialize with default syntax palette size
  m_syntax.token_rgba.resize(static_cast<size_t>(SyntaxToken::Count),
                             0xCBCCC6FF);
}

ThemeSystem::~ThemeSystem() = default;

// -----------------------------------------------------------------------------
// ApplyFromSnapshot
// -----------------------------------------------------------------------------
void ThemeSystem::ApplyFromSnapshot(const ConfigSnapshot &snapshot) {
  const Scheme *scheme = snapshot.registry.Find(snapshot.active_scheme_id);
  if (!scheme) {
    spdlog::warn("[ThemeSystem] Scheme '{}' not found, keeping current theme",
                 snapshot.active_scheme_id);
    return;
  }

  if (m_currentSchemeId == scheme->id) {
    // Same scheme, no change needed
    return;
  }

  m_currentSchemeId = scheme->id;
  m_editor = scheme->editor;
  m_syntax = scheme->syntax;

  // Ensure syntax palette is properly sized
  if (m_syntax.token_rgba.size() < static_cast<size_t>(SyntaxToken::Count)) {
    m_syntax.token_rgba.resize(static_cast<size_t>(SyntaxToken::Count),
                               m_editor.foreground_rgba);
  }

  // Derive ImGui colors
  DeriveImGuiColors(m_editor, scheme->gui_overrides);

  spdlog::info("[ThemeSystem] Applied scheme: {} ({})", scheme->name,
               scheme->variant);
}

// -----------------------------------------------------------------------------
// ColorForToken
// -----------------------------------------------------------------------------
uint32_t ThemeSystem::ColorForToken(SyntaxToken token) const {
  return m_syntax.GetColor(token);
}

// -----------------------------------------------------------------------------
// CaptureToToken - map Tree-sitter capture name to SyntaxToken
// -----------------------------------------------------------------------------
bool ThemeSystem::CaptureToToken(const std::string &capture_name,
                                 SyntaxToken &out_token) {
  // Strip leading @ if present
  std::string name = capture_name;
  if (!name.empty() && name[0] == '@') {
    name = name.substr(1);
  }

  auto it = kCaptureMap.find(name);
  if (it != kCaptureMap.end()) {
    out_token = it->second;
    return true;
  }

  // Try base name (e.g., "string.special" -> "string")
  size_t dot = name.find('.');
  if (dot != std::string::npos) {
    std::string base = name.substr(0, dot);
    it = kCaptureMap.find(base);
    if (it != kCaptureMap.end()) {
      out_token = it->second;
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
// Helper: Convert packed RGBA to ImVec4
// -----------------------------------------------------------------------------
static ImVec4 RGBAToImVec4(uint32_t rgba) {
  float r = ((rgba >> 24) & 0xFF) / 255.0f;
  float g = ((rgba >> 16) & 0xFF) / 255.0f;
  float b = ((rgba >> 8) & 0xFF) / 255.0f;
  float a = (rgba & 0xFF) / 255.0f;
  return ImVec4(r, g, b, a);
}

// -----------------------------------------------------------------------------
// Helper: Lighten or darken a color
// -----------------------------------------------------------------------------
static uint32_t AdjustBrightness(uint32_t rgba, float factor) {
  int r = (rgba >> 24) & 0xFF;
  int g = (rgba >> 16) & 0xFF;
  int b = (rgba >> 8) & 0xFF;
  int a = rgba & 0xFF;

  if (factor > 1.0f) {
    // Lighten
    r = static_cast<int>(r + (255 - r) * (factor - 1.0f));
    g = static_cast<int>(g + (255 - g) * (factor - 1.0f));
    b = static_cast<int>(b + (255 - b) * (factor - 1.0f));
  } else {
    // Darken
    r = static_cast<int>(r * factor);
    g = static_cast<int>(g * factor);
    b = static_cast<int>(b * factor);
  }

  r = std::min(255, std::max(0, r));
  g = std::min(255, std::max(0, g));
  b = std::min(255, std::max(0, b));

  return (r << 24) | (g << 16) | (b << 8) | a;
}

// -----------------------------------------------------------------------------
// DeriveImGuiColors - deterministic mapping from editor palette [DEC-69]
// -----------------------------------------------------------------------------
void ThemeSystem::DeriveImGuiColors(
    const EditorPalette &palette,
    const std::unordered_map<std::string, uint32_t> &overrides) {
  // Resize to fit all ImGuiCol values
  m_imgui.imgui_col_rgba.resize(ImGuiCol_COUNT);

  // Base colors
  uint32_t bg = palette.background_rgba;
  uint32_t fg = palette.foreground_rgba;
  uint32_t accent = palette.caret_rgba;
  uint32_t selection = palette.selection_rgba;

  // Apply overrides if present
  auto getOverride = [&](const char *key, uint32_t def) -> uint32_t {
    auto it = overrides.find(key);
    return it != overrides.end() ? it->second : def;
  };

  uint32_t windowBg = getOverride("window_bg", bg);
  uint32_t text = getOverride("text", fg);
  uint32_t accentColor = getOverride("accent", accent);

  // Derived colors
  uint32_t frameBg = AdjustBrightness(windowBg, 1.15f);
  uint32_t frameBgHovered = AdjustBrightness(frameBg, 1.1f);
  uint32_t frameBgActive = AdjustBrightness(frameBg, 0.9f);

  uint32_t titleBg = AdjustBrightness(windowBg, 0.9f);
  uint32_t titleBgActive = AdjustBrightness(windowBg, 1.1f);

  uint32_t button = AdjustBrightness(accentColor, 0.7f);
  uint32_t buttonHovered = accentColor;
  uint32_t buttonActive = AdjustBrightness(accentColor, 0.8f);

  uint32_t header = AdjustBrightness(selection, 1.0f);
  uint32_t headerHovered = AdjustBrightness(selection, 1.2f);
  uint32_t headerActive = AdjustBrightness(selection, 0.9f);

  // Populate ImGui colors
  m_imgui.imgui_col_rgba[ImGuiCol_Text] = text;
  m_imgui.imgui_col_rgba[ImGuiCol_TextDisabled] = AdjustBrightness(text, 0.5f);
  m_imgui.imgui_col_rgba[ImGuiCol_WindowBg] = windowBg;
  m_imgui.imgui_col_rgba[ImGuiCol_ChildBg] = windowBg;
  m_imgui.imgui_col_rgba[ImGuiCol_PopupBg] = AdjustBrightness(windowBg, 1.05f);
  m_imgui.imgui_col_rgba[ImGuiCol_Border] = AdjustBrightness(windowBg, 1.3f);
  m_imgui.imgui_col_rgba[ImGuiCol_BorderShadow] = 0x00000000;
  m_imgui.imgui_col_rgba[ImGuiCol_FrameBg] = frameBg;
  m_imgui.imgui_col_rgba[ImGuiCol_FrameBgHovered] = frameBgHovered;
  m_imgui.imgui_col_rgba[ImGuiCol_FrameBgActive] = frameBgActive;
  m_imgui.imgui_col_rgba[ImGuiCol_TitleBg] = titleBg;
  m_imgui.imgui_col_rgba[ImGuiCol_TitleBgActive] = titleBgActive;
  m_imgui.imgui_col_rgba[ImGuiCol_TitleBgCollapsed] = titleBg;
  m_imgui.imgui_col_rgba[ImGuiCol_MenuBarBg] = windowBg;
  m_imgui.imgui_col_rgba[ImGuiCol_ScrollbarBg] = windowBg;
  m_imgui.imgui_col_rgba[ImGuiCol_ScrollbarGrab] =
      AdjustBrightness(windowBg, 1.5f);
  m_imgui.imgui_col_rgba[ImGuiCol_ScrollbarGrabHovered] =
      AdjustBrightness(windowBg, 1.7f);
  m_imgui.imgui_col_rgba[ImGuiCol_ScrollbarGrabActive] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_CheckMark] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_SliderGrab] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_SliderGrabActive] =
      AdjustBrightness(accentColor, 1.1f);
  m_imgui.imgui_col_rgba[ImGuiCol_Button] = button;
  m_imgui.imgui_col_rgba[ImGuiCol_ButtonHovered] = buttonHovered;
  m_imgui.imgui_col_rgba[ImGuiCol_ButtonActive] = buttonActive;
  m_imgui.imgui_col_rgba[ImGuiCol_Header] = header;
  m_imgui.imgui_col_rgba[ImGuiCol_HeaderHovered] = headerHovered;
  m_imgui.imgui_col_rgba[ImGuiCol_HeaderActive] = headerActive;
  m_imgui.imgui_col_rgba[ImGuiCol_Separator] = AdjustBrightness(windowBg, 1.3f);
  m_imgui.imgui_col_rgba[ImGuiCol_SeparatorHovered] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_SeparatorActive] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_ResizeGrip] =
      AdjustBrightness(windowBg, 1.3f);
  m_imgui.imgui_col_rgba[ImGuiCol_ResizeGripHovered] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_ResizeGripActive] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_Tab] = AdjustBrightness(windowBg, 1.1f);
  m_imgui.imgui_col_rgba[ImGuiCol_TabHovered] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_TabActive] =
      AdjustBrightness(accentColor, 0.8f);
  m_imgui.imgui_col_rgba[ImGuiCol_TabUnfocused] = windowBg;
  m_imgui.imgui_col_rgba[ImGuiCol_TabUnfocusedActive] =
      AdjustBrightness(windowBg, 1.2f);
  m_imgui.imgui_col_rgba[ImGuiCol_DockingPreview] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_DockingEmptyBg] = windowBg;
  m_imgui.imgui_col_rgba[ImGuiCol_PlotLines] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_PlotLinesHovered] =
      AdjustBrightness(accentColor, 1.2f);
  m_imgui.imgui_col_rgba[ImGuiCol_PlotHistogram] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_PlotHistogramHovered] =
      AdjustBrightness(accentColor, 1.2f);
  m_imgui.imgui_col_rgba[ImGuiCol_TableHeaderBg] =
      AdjustBrightness(windowBg, 1.1f);
  m_imgui.imgui_col_rgba[ImGuiCol_TableBorderStrong] =
      AdjustBrightness(windowBg, 1.3f);
  m_imgui.imgui_col_rgba[ImGuiCol_TableBorderLight] =
      AdjustBrightness(windowBg, 1.2f);
  m_imgui.imgui_col_rgba[ImGuiCol_TableRowBg] = windowBg;
  m_imgui.imgui_col_rgba[ImGuiCol_TableRowBgAlt] =
      AdjustBrightness(windowBg, 1.05f);
  m_imgui.imgui_col_rgba[ImGuiCol_TextSelectedBg] = selection;
  m_imgui.imgui_col_rgba[ImGuiCol_DragDropTarget] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_NavHighlight] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_NavWindowingHighlight] = accentColor;
  m_imgui.imgui_col_rgba[ImGuiCol_NavWindowingDimBg] = 0x33333380;
  m_imgui.imgui_col_rgba[ImGuiCol_ModalWindowDimBg] = 0x33333380;
}

// -----------------------------------------------------------------------------
// ApplyImGuiTheme - apply colors to ImGui style
// -----------------------------------------------------------------------------
void ThemeSystem::ApplyImGuiTheme() {
  struct ImGuiStyle &style = ImGui::GetStyle();

  for (size_t i = 0; i < m_imgui.imgui_col_rgba.size() && i < ImGuiCol_COUNT;
       ++i) {
    style.Colors[i] = RGBAToImVec4(m_imgui.imgui_col_rgba[i]);
  }

  // Additional style settings
  style.WindowRounding = 6.0f;
  style.FrameRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;
}

} // namespace arcanee::ide::config
