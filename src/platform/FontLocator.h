// ARCANEE Font Locator - [API-22]
// Abstract interface for platform font discovery.
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#pragma once
#include "ide/config/ConfigSnapshot.h"
#include <memory>
#include <string>

namespace arcanee::platform {

// Forward reference namespace
using FontSpec = arcanee::ide::config::FontSpec;
using FontWeight = arcanee::ide::config::FontWeight;
using FontStyle = arcanee::ide::config::FontStyle;

// -----------------------------------------------------------------------------
// Font load result
// -----------------------------------------------------------------------------
struct FontLoadResult {
  bool ok = false;
  std::string message; // For diagnostics on failure
};

// -----------------------------------------------------------------------------
// FontLocator - abstract interface for system font discovery [REQ-92]
// -----------------------------------------------------------------------------
class FontLocator {
public:
  virtual ~FontLocator() = default;

  // Resolve font family to file path (for ImGui loading)
  // Returns empty string if not found
  virtual std::string GetFontPath(const std::string &family,
                                  FontWeight weight = FontWeight::Regular,
                                  FontStyle style = FontStyle::Normal) = 0;

  // Apply editor font specification
  virtual FontLoadResult ApplyEditorFont(const FontSpec &spec) = 0;

  // Apply UI font specification
  virtual FontLoadResult ApplyUiFont(const FontSpec &spec) = 0;

  // Query if a font family is available
  virtual bool IsFamilyAvailable(const std::string &family) = 0;

  // Get system default monospace font family
  virtual std::string GetDefaultMonospaceFamily() = 0;

  // Get system default UI font family
  virtual std::string GetDefaultUiFamily() = 0;
};

// -----------------------------------------------------------------------------
// Factory function - creates platform-appropriate FontLocator
// -----------------------------------------------------------------------------
std::unique_ptr<FontLocator> CreateFontLocator();

} // namespace arcanee::platform
