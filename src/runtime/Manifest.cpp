/**
 * @file Manifest.cpp
 * @brief Cartridge manifest parsing implementation.
 *
 * Implements a simple TOML parser sufficient for cartridge.toml manifests.
 * This is a minimal implementation focused on the subset of TOML needed
 * for v0.1 manifests.
 *
 * @ref specs/Chapter 3 §3.4
 *      "Manifest Specification (v0.1)"
 */

#include "Manifest.h"
#include "common/Log.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace arcanee::runtime {

// ============================================================================
// Simple TOML Value Types
// ============================================================================

struct TomlValue {
  enum class Type { String, Integer, Float, Boolean, None };
  Type type = Type::None;
  std::string stringVal;
  int64_t intVal = 0;
  double floatVal = 0.0;
  bool boolVal = false;

  bool isString() const { return type == Type::String; }
  bool isInt() const { return type == Type::Integer; }
  bool isFloat() const { return type == Type::Float; }
  bool isBool() const { return type == Type::Boolean; }
};

using TomlTable = std::unordered_map<std::string, TomlValue>;
using TomlDocument = std::unordered_map<std::string, TomlTable>;

// ============================================================================
// TOML Parser (Minimal implementation for manifest subset)
// ============================================================================

class TomlParser {
public:
  TomlParser(const std::string &content) : m_content(content), m_pos(0) {}

  bool parse() {
    m_line = 1;
    m_rootTable.clear();
    m_tables.clear();
    m_currentTable = &m_rootTable;
    m_currentTableName = "";

    while (!atEnd()) {
      skipWhitespaceAndComments();
      if (atEnd())
        break;

      char c = peek();
      if (c == '[') {
        if (!parseTableHeader()) {
          return false;
        }
      } else if (std::isalpha(c) || c == '_') {
        if (!parseKeyValue()) {
          return false;
        }
      } else if (c == '\n' || c == '\r') {
        advance();
        if (c == '\r' && peek() == '\n')
          advance();
        m_line++;
      } else {
        m_errorLine = m_line;
        m_errorMessage = "Unexpected character";
        return false;
      }
    }
    return true;
  }

  TomlValue getValue(const std::string &tableName,
                     const std::string &key) const {
    if (tableName.empty()) {
      auto it = m_rootTable.find(key);
      if (it != m_rootTable.end())
        return it->second;
    } else {
      auto tableIt = m_tables.find(tableName);
      if (tableIt != m_tables.end()) {
        auto it = tableIt->second.find(key);
        if (it != tableIt->second.end())
          return it->second;
      }
    }
    return TomlValue{};
  }

  std::string getString(const std::string &table, const std::string &key,
                        const std::string &defaultVal = "") const {
    auto val = getValue(table, key);
    if (val.isString())
      return val.stringVal;
    return defaultVal;
  }

  bool getBool(const std::string &table, const std::string &key,
               bool defaultVal = false) const {
    auto val = getValue(table, key);
    if (val.isBool())
      return val.boolVal;
    return defaultVal;
  }

  int64_t getInt(const std::string &table, const std::string &key,
                 int64_t defaultVal = 0) const {
    auto val = getValue(table, key);
    if (val.isInt())
      return val.intVal;
    return defaultVal;
  }

  double getFloat(const std::string &table, const std::string &key,
                  double defaultVal = 0.0) const {
    auto val = getValue(table, key);
    if (val.isFloat())
      return val.floatVal;
    if (val.isInt())
      return static_cast<double>(val.intVal);
    return defaultVal;
  }

  bool hasKey(const std::string &table, const std::string &key) const {
    if (table.empty()) {
      return m_rootTable.find(key) != m_rootTable.end();
    } else {
      auto tableIt = m_tables.find(table);
      if (tableIt != m_tables.end()) {
        return tableIt->second.find(key) != tableIt->second.end();
      }
    }
    return false;
  }

  int getErrorLine() const { return m_errorLine; }
  const std::string &getErrorMessage() const { return m_errorMessage; }

private:
  bool atEnd() const { return m_pos >= m_content.size(); }

  char peek() const {
    if (atEnd())
      return '\0';
    return m_content[m_pos];
  }

  char advance() {
    if (atEnd())
      return '\0';
    return m_content[m_pos++];
  }

  void skipWhitespace() {
    while (!atEnd() && (peek() == ' ' || peek() == '\t')) {
      advance();
    }
  }

  void skipWhitespaceAndComments() {
    while (!atEnd()) {
      skipWhitespace();
      if (peek() == '#') {
        // Skip comment to end of line
        while (!atEnd() && peek() != '\n' && peek() != '\r') {
          advance();
        }
      } else if (peek() == '\n' || peek() == '\r') {
        char c = advance();
        if (c == '\r' && peek() == '\n')
          advance();
        m_line++;
      } else {
        break;
      }
    }
  }

  bool parseTableHeader() {
    // [table_name]
    advance(); // consume '['
    skipWhitespace();

    std::string tableName;
    while (!atEnd() && peek() != ']' && peek() != '\n') {
      tableName += advance();
    }

    // Trim trailing whitespace
    while (!tableName.empty() && std::isspace(tableName.back())) {
      tableName.pop_back();
    }

    if (peek() != ']') {
      m_errorLine = m_line;
      m_errorMessage = "Expected ']' in table header";
      return false;
    }
    advance(); // consume ']'

    m_currentTableName = tableName;
    m_tables[tableName] = TomlTable{};
    m_currentTable = &m_tables[tableName];

    return true;
  }

  bool parseKeyValue() {
    // key = value
    std::string key;
    while (!atEnd() && (std::isalnum(peek()) || peek() == '_')) {
      key += advance();
    }

    skipWhitespace();
    if (peek() != '=') {
      m_errorLine = m_line;
      m_errorMessage = "Expected '=' after key";
      return false;
    }
    advance(); // consume '='
    skipWhitespace();

    TomlValue value;
    if (!parseValue(value)) {
      return false;
    }

    (*m_currentTable)[key] = value;
    return true;
  }

  bool parseValue(TomlValue &value) {
    char c = peek();

    if (c == '"') {
      // String
      return parseString(value);
    } else if (c == 't' || c == 'f') {
      // Boolean
      return parseBoolean(value);
    } else if (std::isdigit(c) || c == '-' || c == '+') {
      // Number
      return parseNumber(value);
    } else {
      m_errorLine = m_line;
      m_errorMessage = "Unexpected value type";
      return false;
    }
  }

  bool parseString(TomlValue &value) {
    advance(); // consume opening '"'
    std::string str;

    while (!atEnd() && peek() != '"') {
      char c = advance();
      if (c == '\\') {
        // Escape sequence
        if (atEnd()) {
          m_errorLine = m_line;
          m_errorMessage = "Unterminated string";
          return false;
        }
        char escaped = advance();
        switch (escaped) {
        case 'n':
          str += '\n';
          break;
        case 't':
          str += '\t';
          break;
        case 'r':
          str += '\r';
          break;
        case '"':
          str += '"';
          break;
        case '\\':
          str += '\\';
          break;
        default:
          str += escaped;
          break;
        }
      } else if (c == '\n' || c == '\r') {
        m_errorLine = m_line;
        m_errorMessage = "Newline in string";
        return false;
      } else {
        str += c;
      }
    }

    if (peek() != '"') {
      m_errorLine = m_line;
      m_errorMessage = "Unterminated string";
      return false;
    }
    advance(); // consume closing '"'

    value.type = TomlValue::Type::String;
    value.stringVal = str;
    return true;
  }

  bool parseBoolean(TomlValue &value) {
    if (m_content.compare(m_pos, 4, "true") == 0) {
      m_pos += 4;
      value.type = TomlValue::Type::Boolean;
      value.boolVal = true;
      return true;
    } else if (m_content.compare(m_pos, 5, "false") == 0) {
      m_pos += 5;
      value.type = TomlValue::Type::Boolean;
      value.boolVal = false;
      return true;
    }
    m_errorLine = m_line;
    m_errorMessage = "Invalid boolean";
    return false;
  }

  bool parseNumber(TomlValue &value) {
    size_t start = m_pos;
    bool isFloat = false;

    if (peek() == '-' || peek() == '+') {
      advance();
    }

    while (!atEnd() && std::isdigit(peek())) {
      advance();
    }

    if (peek() == '.') {
      isFloat = true;
      advance();
      while (!atEnd() && std::isdigit(peek())) {
        advance();
      }
    }

    std::string numStr = m_content.substr(start, m_pos - start);

    if (isFloat) {
      value.type = TomlValue::Type::Float;
      value.floatVal = std::stod(numStr);
    } else {
      value.type = TomlValue::Type::Integer;
      value.intVal = std::stoll(numStr);
    }
    return true;
  }

  std::string m_content;
  size_t m_pos;
  int m_line = 1;
  int m_errorLine = 0;
  std::string m_errorMessage;

  TomlTable m_rootTable;
  TomlDocument m_tables;
  TomlTable *m_currentTable;
  std::string m_currentTableName;
};

// ============================================================================
// Manifest Parsing
// ============================================================================

/**
 * @brief Parse aspect ratio string.
 */
static std::optional<Aspect> parseAspect(const std::string &str) {
  if (str == "16:9")
    return Aspect::Ratio16x9;
  if (str == "4:3")
    return Aspect::Ratio4x3;
  if (str == "any")
    return Aspect::Any;
  return std::nullopt;
}

/**
 * @brief Parse preset string.
 */
static std::optional<Preset> parsePreset(const std::string &str) {
  if (str == "low")
    return Preset::Low;
  if (str == "medium")
    return Preset::Medium;
  if (str == "high")
    return Preset::High;
  if (str == "ultra")
    return Preset::Ultra;
  return std::nullopt;
}

/**
 * @brief Parse scaling string.
 */
static std::optional<Scaling> parseScaling(const std::string &str) {
  if (str == "fit")
    return Scaling::Fit;
  if (str == "integer_nearest")
    return Scaling::IntegerNearest;
  if (str == "fill")
    return Scaling::Fill;
  if (str == "stretch")
    return Scaling::Stretch;
  return std::nullopt;
}

ManifestResult parseManifest(const std::string &content) {
  TomlParser parser(content);

  if (!parser.parse()) {
    return ManifestError{parser.getErrorLine(), parser.getErrorMessage()};
  }

  Manifest manifest;

  // Required root fields
  // @ref specs/Chapter 3 §3.4.1
  manifest.id = parser.getString("", "id");
  manifest.title = parser.getString("", "title");
  manifest.version = parser.getString("", "version");
  manifest.apiVersion = parser.getString("", "api_version");
  manifest.entry = parser.getString("", "entry");

  // Display section
  // @ref specs/Chapter 3 §3.4.2
  std::string aspectStr = parser.getString("display", "aspect", "16:9");
  auto aspect = parseAspect(aspectStr);
  if (aspect) {
    manifest.display.aspect = *aspect;
  } else {
    return ManifestError{0, "Invalid display.aspect: " + aspectStr};
  }

  std::string presetStr = parser.getString("display", "preset", "medium");
  auto preset = parsePreset(presetStr);
  if (preset) {
    manifest.display.preset = *preset;
  } else {
    return ManifestError{0, "Invalid display.preset: " + presetStr};
  }

  std::string scalingStr = parser.getString("display", "scaling", "fit");
  auto scaling = parseScaling(scalingStr);
  if (scaling) {
    manifest.display.scaling = *scaling;
  } else {
    return ManifestError{0, "Invalid display.scaling: " + scalingStr};
  }

  manifest.display.allowUserOverride =
      parser.getBool("display", "allow_user_override", true);

  // Permissions section
  // @ref specs/Chapter 3 §3.4.3
  manifest.permissions.saveStorage =
      parser.getBool("permissions", "save_storage", true);
  manifest.permissions.audio = parser.getBool("permissions", "audio", true);
  manifest.permissions.net = parser.getBool("permissions", "net", false);
  manifest.permissions.native = parser.getBool("permissions", "native", false);

  // Caps section (optional)
  // @ref specs/Chapter 3 §3.4.4
  manifest.caps.cpuMsPerUpdate =
      static_cast<float>(parser.getFloat("caps", "cpu_ms_per_update", 2.0));
  manifest.caps.vmMemoryMb =
      static_cast<int>(parser.getInt("caps", "vm_memory_mb", 64));
  manifest.caps.maxDrawCalls =
      static_cast<int>(parser.getInt("caps", "max_draw_calls", 20000));
  manifest.caps.maxCanvasPixels =
      static_cast<int>(parser.getInt("caps", "max_canvas_pixels", 16777216));
  manifest.caps.audioChannels =
      static_cast<int>(parser.getInt("caps", "audio_channels", 32));

  return manifest;
}

std::optional<std::string> validateManifest(const Manifest &manifest) {
  // Required fields check
  // @ref specs/Chapter 3 §3.4.1
  if (manifest.id.empty()) {
    return "Missing required field: id";
  }
  if (manifest.title.empty()) {
    return "Missing required field: title";
  }
  if (manifest.version.empty()) {
    return "Missing required field: version";
  }
  if (manifest.apiVersion.empty()) {
    return "Missing required field: api_version";
  }
  if (manifest.entry.empty()) {
    return "Missing required field: entry";
  }

  // API version check
  // @ref specs/Chapter 3 §3.4.1: "MUST be \"0.1\" for v0.1"
  if (manifest.apiVersion != "0.1") {
    return "Unsupported api_version: " + manifest.apiVersion +
           " (expected \"0.1\")";
  }

  return std::nullopt;
}

// ============================================================================
// Utility Functions
// ============================================================================

void getCbufDimensions(const DisplayConfig &display, int &outWidth,
                       int &outHeight) {
  // @ref specs/Chapter 5 §5.3.1: CBUF Preset table
  if (display.aspect == Aspect::Ratio4x3) {
    switch (display.preset) {
    case Preset::Low:
      outWidth = 400;
      outHeight = 300;
      break;
    case Preset::Medium:
      outWidth = 800;
      outHeight = 600;
      break;
    case Preset::High:
      outWidth = 1600;
      outHeight = 1200;
      break;
    case Preset::Ultra:
      outWidth = 3200;
      outHeight = 2400;
      break;
    }
  } else {
    // 16:9 or Any
    switch (display.preset) {
    case Preset::Low:
      outWidth = 480;
      outHeight = 270;
      break;
    case Preset::Medium:
      outWidth = 960;
      outHeight = 540;
      break;
    case Preset::High:
      outWidth = 1920;
      outHeight = 1080;
      break;
    case Preset::Ultra:
      outWidth = 3840;
      outHeight = 2160;
      break;
    }
  }
}

const char *aspectToString(Aspect aspect) {
  switch (aspect) {
  case Aspect::Ratio16x9:
    return "16:9";
  case Aspect::Ratio4x3:
    return "4:3";
  case Aspect::Any:
    return "any";
  }
  return "unknown";
}

const char *presetToString(Preset preset) {
  switch (preset) {
  case Preset::Low:
    return "low";
  case Preset::Medium:
    return "medium";
  case Preset::High:
    return "high";
  case Preset::Ultra:
    return "ultra";
  }
  return "unknown";
}

const char *scalingToString(Scaling scaling) {
  switch (scaling) {
  case Scaling::Fit:
    return "fit";
  case Scaling::IntegerNearest:
    return "integer_nearest";
  case Scaling::Fill:
    return "fill";
  case Scaling::Stretch:
    return "stretch";
  }
  return "unknown";
}

} // namespace arcanee::runtime
