#pragma once

/**
 * @file Manifest.h
 * @brief Cartridge manifest parsing and validation.
 *
 * Parses cartridge.toml manifest files per Chapter 3 §3.4 specification.
 * Validates required fields and provides sensible defaults for optional fields.
 *
 * @ref specs/Chapter 3 §3.4
 *      "Manifest Specification (v0.1)"
 */

#include "common/Types.h"
#include <optional>
#include <string>
#include <variant>

namespace arcanee::runtime {

// ============================================================================
// Manifest data structures (Chapter 3 §3.4)
// ============================================================================

/**
 * @brief Display aspect ratio.
 *
 * @ref specs/Chapter 3 §3.4.2
 *      "aspect = \"16:9\" | \"4:3\" | \"any\""
 */
enum class Aspect { Ratio16x9, Ratio4x3, Any };

/**
 * @brief Display resolution preset.
 *
 * @ref specs/Chapter 3 §3.4.2
 *      "preset = \"low\" | \"medium\" | \"high\" | \"ultra\""
 */
enum class Preset { Low, Medium, High, Ultra };

/**
 * @brief Display scaling mode.
 *
 * @ref specs/Chapter 3 §3.4.2
 *      "scaling = \"fit\" | \"integer_nearest\" | \"fill\" | \"stretch\""
 */
enum class Scaling { Fit, IntegerNearest, Fill, Stretch };

/**
 * @brief Display configuration from manifest.
 *
 * @ref specs/Chapter 3 §3.4.2
 */
struct DisplayConfig {
  Aspect aspect = Aspect::Ratio16x9;
  Preset preset = Preset::Medium;
  Scaling scaling = Scaling::Fit;
  bool allowUserOverride = true;
};

/**
 * @brief Permission flags from manifest.
 *
 * @ref specs/Chapter 3 §3.4.3
 */
struct Permissions {
  bool saveStorage = true;
  bool audio = true;
  bool net = false;    ///< v0.1 does not support networking
  bool native = false; ///< v0.1 does not support native plugins
};

/**
 * @brief Capability hints from manifest.
 *
 * @ref specs/Chapter 3 §3.4.4
 *      "caps provides hints; the runtime is authoritative"
 */
struct Caps {
  float cpuMsPerUpdate = 2.0f;
  int vmMemoryMb = 64;
  int maxDrawCalls = 20000;
  int maxCanvasPixels = 16777216;
  int audioChannels = 32;
};

/**
 * @brief Complete manifest data.
 *
 * @ref specs/Chapter 3 §3.4.1
 */
struct Manifest {
  // Required fields
  std::string id;         ///< Stable unique ID; used for save mapping
  std::string title;      ///< User-facing title
  std::string version;    ///< Semver recommended
  std::string apiVersion; ///< MUST be "0.1" for v0.1
  std::string entry;      ///< Entry script path relative to cart:/

  // Required sections
  DisplayConfig display;
  Permissions permissions;

  // Optional
  Caps caps;
};

/**
 * @brief Parse error information.
 */
struct ManifestError {
  int line = 0;        ///< Line number (0 if unknown)
  std::string message; ///< Error description
};

// ============================================================================
// Manifest Parser
// ============================================================================

/**
 * @brief Result type for manifest parsing.
 */
using ManifestResult = std::variant<Manifest, ManifestError>;

/**
 * @brief Parse a cartridge manifest from TOML content.
 *
 * @param content TOML file content as string.
 * @return Parsed manifest or error.
 *
 * @ref specs/Chapter 3 §3.4
 *      "Manifest Specification (v0.1)"
 */
ManifestResult parseManifest(const std::string &content);

/**
 * @brief Validate a parsed manifest.
 *
 * Checks:
 * - All required fields are present
 * - api_version is "0.1"
 * - Field values are within valid ranges
 *
 * @param manifest Manifest to validate.
 * @return nullopt if valid, error message if invalid.
 */
std::optional<std::string> validateManifest(const Manifest &manifest);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get CBUF dimensions for a display configuration.
 *
 * @ref specs/Chapter 5 §5.3.1
 */
void getCbufDimensions(const DisplayConfig &display, int &outWidth,
                       int &outHeight);

/**
 * @brief Convert aspect enum to string.
 */
const char *aspectToString(Aspect aspect);

/**
 * @brief Convert preset enum to string.
 */
const char *presetToString(Preset preset);

/**
 * @brief Convert scaling enum to string.
 */
const char *scalingToString(Scaling scaling);

} // namespace arcanee::runtime
