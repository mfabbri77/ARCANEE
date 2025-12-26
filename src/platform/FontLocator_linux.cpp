// ARCANEE Font Locator - Linux Implementation (fontconfig)
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#include "FontLocator.h"
#include <spdlog/spdlog.h>

#ifdef __linux__
// fontconfig is optional on Linux [DEC-72]
#ifdef ARCANEE_HAS_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif
#endif

namespace arcanee::platform {

#ifdef __linux__

class FontLocatorLinux : public FontLocator {
public:
  FontLocatorLinux() {
#ifdef ARCANEE_HAS_FONTCONFIG
    m_fcConfig = FcInitLoadConfigAndFonts();
    if (!m_fcConfig) {
      spdlog::warn(
          "[FontLocator] fontconfig initialization failed, using fallback");
    } else {
      spdlog::info("[FontLocator] fontconfig initialized");
    }
#else
    spdlog::info(
        "[FontLocator] fontconfig not available, using fallback fonts");
#endif
  }

  ~FontLocatorLinux() override {
#ifdef ARCANEE_HAS_FONTCONFIG
    if (m_fcConfig) {
      FcConfigDestroy(m_fcConfig);
    }
    FcFini();
#endif
  }

  std::string GetFontPath(const std::string &family, FontWeight weight,
                          FontStyle style) override {
#ifdef ARCANEE_HAS_FONTCONFIG
    if (!m_fcConfig)
      return "";

    // Build pattern with family, weight, style
    FcPattern *pattern = FcPatternCreate();
    if (!pattern)
      return "";

    FcPatternAddString(pattern, FC_FAMILY,
                       reinterpret_cast<const FcChar8 *>(family.c_str()));

    // Map weight
    int fcWeight = FC_WEIGHT_REGULAR;
    switch (weight) {
    case FontWeight::Light:
      fcWeight = FC_WEIGHT_LIGHT;
      break;
    case FontWeight::Regular:
      fcWeight = FC_WEIGHT_REGULAR;
      break;
    case FontWeight::Medium:
      fcWeight = FC_WEIGHT_MEDIUM;
      break;
    case FontWeight::SemiBold:
      fcWeight = FC_WEIGHT_SEMIBOLD;
      break;
    case FontWeight::Bold:
      fcWeight = FC_WEIGHT_BOLD;
      break;
    }
    FcPatternAddInteger(pattern, FC_WEIGHT, fcWeight);

    // Map style
    int fcSlant =
        (style == FontStyle::Italic) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
    FcPatternAddInteger(pattern, FC_SLANT, fcSlant);

    FcConfigSubstitute(m_fcConfig, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult fcResult;
    FcPattern *match = FcFontMatch(m_fcConfig, pattern, &fcResult);
    FcPatternDestroy(pattern);

    if (!match)
      return "";

    FcChar8 *filePath = nullptr;
    if (FcPatternGetString(match, FC_FILE, 0, &filePath) == FcResultMatch &&
        filePath) {
      std::string result(reinterpret_cast<char *>(filePath));
      FcPatternDestroy(match);
      spdlog::debug("[FontLocator] Resolved {} -> {}", family, result);
      return result;
    }

    FcPatternDestroy(match);
#else
    (void)family;
    (void)weight;
    (void)style;
#endif
    return "";
  }

  FontLoadResult ApplyEditorFont(const FontSpec &spec) override {
    FontLoadResult result;

    // Validate: no file paths allowed [REQ-92]
    if (spec.family.find('/') != std::string::npos) {
      result.ok = false;
      result.message = "Font file paths not allowed";
      spdlog::warn("[FontLocator] Font path rejected: {}", spec.family);
      return result;
    }

#ifdef ARCANEE_HAS_FONTCONFIG
    if (m_fcConfig) {
      bool found = IsFamilyAvailable(spec.family);
      if (found) {
        m_currentEditorFamily = spec.family;
        result.ok = true;
        spdlog::info("[FontLocator] Editor font set: {} {}px", spec.family,
                     spec.size_px);
      } else {
        // Fallback to default monospace
        m_currentEditorFamily = GetDefaultMonospaceFamily();
        result.ok = true;
        result.message =
            "Family not found, using fallback: " + m_currentEditorFamily;
        spdlog::warn("[FontLocator] {} not found, using {}", spec.family,
                     m_currentEditorFamily);
      }
      return result;
    }
#endif

    // Fallback without fontconfig
    m_currentEditorFamily = "monospace";
    result.ok = true;
    result.message = "fontconfig unavailable, using system default";
    return result;
  }

  FontLoadResult ApplyUiFont(const FontSpec &spec) override {
    FontLoadResult result;

    if (spec.family.find('/') != std::string::npos) {
      result.ok = false;
      result.message = "Font file paths not allowed";
      return result;
    }

#ifdef ARCANEE_HAS_FONTCONFIG
    if (m_fcConfig) {
      bool found = IsFamilyAvailable(spec.family);
      if (found) {
        m_currentUiFamily = spec.family;
        result.ok = true;
      } else {
        m_currentUiFamily = GetDefaultUiFamily();
        result.ok = true;
        result.message =
            "Family not found, using fallback: " + m_currentUiFamily;
      }
      return result;
    }
#endif

    m_currentUiFamily = "sans-serif";
    result.ok = true;
    result.message = "fontconfig unavailable, using system default";
    return result;
  }

  bool IsFamilyAvailable(const std::string &family) override {
#ifdef ARCANEE_HAS_FONTCONFIG
    if (!m_fcConfig)
      return false;

    FcPattern *pattern =
        FcNameParse(reinterpret_cast<const FcChar8 *>(family.c_str()));
    if (!pattern)
      return false;

    FcConfigSubstitute(m_fcConfig, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult fcResult;
    FcPattern *match = FcFontMatch(m_fcConfig, pattern, &fcResult);
    FcPatternDestroy(pattern);

    if (!match)
      return false;

    FcChar8 *matchedFamily = nullptr;
    if (FcPatternGetString(match, FC_FAMILY, 0, &matchedFamily) ==
        FcResultMatch) {
      // Check if the matched family is close to requested
      std::string matched(reinterpret_cast<char *>(matchedFamily));
      FcPatternDestroy(match);
      // fontconfig always returns something, check if it's the right family
      return (matched.find(family) != std::string::npos ||
              family.find(matched) != std::string::npos);
    }

    FcPatternDestroy(match);
    return false;
#else
    (void)family;
    return false;
#endif
  }

  std::string GetDefaultMonospaceFamily() override {
#ifdef ARCANEE_HAS_FONTCONFIG
    if (m_fcConfig) {
      // Try common monospace fonts
      const char *candidates[] = {"JetBrains Mono",  "Fira Code",
                                  "Source Code Pro", "DejaVu Sans Mono",
                                  "Liberation Mono", "Monospace"};
      for (const char *candidate : candidates) {
        if (IsFamilyAvailable(candidate)) {
          return candidate;
        }
      }
    }
#endif
    return "monospace";
  }

  std::string GetDefaultUiFamily() override {
#ifdef ARCANEE_HAS_FONTCONFIG
    if (m_fcConfig) {
      const char *candidates[] = {"Inter",       "Roboto",          "Noto Sans",
                                  "DejaVu Sans", "Liberation Sans", "Sans"};
      for (const char *candidate : candidates) {
        if (IsFamilyAvailable(candidate)) {
          return candidate;
        }
      }
    }
#endif
    return "sans-serif";
  }

private:
#ifdef ARCANEE_HAS_FONTCONFIG
  FcConfig *m_fcConfig = nullptr;
#endif
  std::string m_currentEditorFamily = "monospace";
  std::string m_currentUiFamily = "sans-serif";
};

#endif // __linux__

// -----------------------------------------------------------------------------
// Factory function
// -----------------------------------------------------------------------------
std::unique_ptr<FontLocator> CreateFontLocator() {
#ifdef __linux__
  return std::make_unique<FontLocatorLinux>();
#elif defined(_WIN32)
  // Windows stub - TODO: DirectWrite implementation
  spdlog::warn("[FontLocator] Windows DirectWrite not yet implemented");
  return nullptr;
#elif defined(__APPLE__)
  // macOS stub - TODO: CoreText implementation
  spdlog::warn("[FontLocator] macOS CoreText not yet implemented");
  return nullptr;
#else
  return nullptr;
#endif
}

} // namespace arcanee::platform
