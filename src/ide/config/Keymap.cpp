// ARCANEE Keymap - Implementation
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#include "Keymap.h"
#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>
#include <sstream>
#include <unordered_map>

namespace arcanee::ide::config {

// -----------------------------------------------------------------------------
// Key name to scancode mapping (subset of common keys)
// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, uint16_t> kKeyNameToCode = {
    // Letters
    {"a", 'A'},
    {"b", 'B'},
    {"c", 'C'},
    {"d", 'D'},
    {"e", 'E'},
    {"f", 'F'},
    {"g", 'G'},
    {"h", 'H'},
    {"i", 'I'},
    {"j", 'J'},
    {"k", 'K'},
    {"l", 'L'},
    {"m", 'M'},
    {"n", 'N'},
    {"o", 'O'},
    {"p", 'P'},
    {"q", 'Q'},
    {"r", 'R'},
    {"s", 'S'},
    {"t", 'T'},
    {"u", 'U'},
    {"v", 'V'},
    {"w", 'W'},
    {"x", 'X'},
    {"y", 'Y'},
    {"z", 'Z'},

    // Numbers
    {"0", '0'},
    {"1", '1'},
    {"2", '2'},
    {"3", '3'},
    {"4", '4'},
    {"5", '5'},
    {"6", '6'},
    {"7", '7'},
    {"8", '8'},
    {"9", '9'},

    // Function keys
    {"f1", 0x70},
    {"f2", 0x71},
    {"f3", 0x72},
    {"f4", 0x73},
    {"f5", 0x74},
    {"f6", 0x75},
    {"f7", 0x76},
    {"f8", 0x77},
    {"f9", 0x78},
    {"f10", 0x79},
    {"f11", 0x7A},
    {"f12", 0x7B},

    // Special keys
    {"enter", 0x0D},
    {"return", 0x0D},
    {"escape", 0x1B},
    {"esc", 0x1B},
    {"tab", 0x09},
    {"space", 0x20},
    {"backspace", 0x08},
    {"delete", 0x2E},
    {"del", 0x2E},
    {"insert", 0x2D},
    {"ins", 0x2D},
    {"home", 0x24},
    {"end", 0x23},
    {"pageup", 0x21},
    {"pgup", 0x21},
    {"pagedown", 0x22},
    {"pgdn", 0x22},
    {"up", 0x26},
    {"down", 0x28},
    {"left", 0x25},
    {"right", 0x27},

    // Punctuation
    {"-", '-'},
    {"minus", '-'},
    {"=", '='},
    {"equals", '='},
    {"[", '['},
    {"]", ']'},
    {";", ';'},
    {"'", '\''},
    {",", ','},
    {".", '.'},
    {"/", '/'},
    {"\\", '\\'},
    {"`", '`'},
};

// -----------------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------------
Keymap::Keymap() = default;
Keymap::~Keymap() = default;

// -----------------------------------------------------------------------------
// IsMacOS
// -----------------------------------------------------------------------------
bool Keymap::IsMacOS() {
#ifdef __APPLE__
  return true;
#else
  return false;
#endif
}

// -----------------------------------------------------------------------------
// ParseChordString - parse "primary+Shift+S" into packed chord
// Format: modifiers(8 bits) | keycode(16 bits) | reserved(8 bits)
// -----------------------------------------------------------------------------
uint32_t Keymap::ParseChordString(const std::string &chord_str) {
  if (chord_str.empty()) {
    return 0;
  }

  // Split by '+'
  std::vector<std::string> parts;
  std::stringstream ss(chord_str);
  std::string part;
  while (std::getline(ss, part, '+')) {
    // Trim whitespace and convert to lowercase
    part.erase(0, part.find_first_not_of(" \t"));
    part.erase(part.find_last_not_of(" \t") + 1);
    std::transform(part.begin(), part.end(), part.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (!part.empty()) {
      parts.push_back(part);
    }
  }

  if (parts.empty()) {
    return 0;
  }

  ModifierFlags mods = ModifierFlags::None;
  uint16_t keycode = 0;

  for (const auto &p : parts) {
    // Check for modifiers
    if (p == "ctrl" || p == "control") {
      mods = mods | ModifierFlags::Ctrl;
    } else if (p == "alt" || p == "option") {
      mods = mods | ModifierFlags::Alt;
    } else if (p == "shift") {
      mods = mods | ModifierFlags::Shift;
    } else if (p == "super" || p == "win" || p == "cmd" || p == "command" ||
               p == "meta") {
      mods = mods | ModifierFlags::Super;
    } else if (p == "primary") {
      // Platform-specific: Cmd on macOS, Ctrl elsewhere [REQ-96]
      if (IsMacOS()) {
        mods = mods | ModifierFlags::Super;
      } else {
        mods = mods | ModifierFlags::Ctrl;
      }
    } else {
      // Must be the key
      auto it = kKeyNameToCode.find(p);
      if (it != kKeyNameToCode.end()) {
        keycode = it->second;
      } else if (p.length() == 1) {
        // Single character key
        keycode = static_cast<uint16_t>(std::toupper(p[0]));
      } else {
        spdlog::warn("[Keymap] Unknown key name: '{}'", p);
        return 0;
      }
    }
  }

  if (keycode == 0) {
    spdlog::warn("[Keymap] No key specified in chord: '{}'", chord_str);
    return 0;
  }

  // Pack: modifiers(8) | keycode(16) | 0(8)
  return (static_cast<uint32_t>(mods) << 24) |
         (static_cast<uint32_t>(keycode) << 8);
}

// -----------------------------------------------------------------------------
// FormatChord - format packed chord for display
// -----------------------------------------------------------------------------
std::string Keymap::FormatChord(uint32_t chord_packed) {
  if (chord_packed == 0) {
    return "";
  }

  auto mods = static_cast<ModifierFlags>((chord_packed >> 24) & 0xFF);
  uint16_t keycode = (chord_packed >> 8) & 0xFFFF;

  std::string result;

  if (HasMod(mods, ModifierFlags::Ctrl)) {
    result += IsMacOS() ? "⌃" : "Ctrl+";
  }
  if (HasMod(mods, ModifierFlags::Alt)) {
    result += IsMacOS() ? "⌥" : "Alt+";
  }
  if (HasMod(mods, ModifierFlags::Shift)) {
    result += IsMacOS() ? "⇧" : "Shift+";
  }
  if (HasMod(mods, ModifierFlags::Super)) {
    result += IsMacOS() ? "⌘" : "Win+";
  }

  // Find key name
  for (const auto &[name, code] : kKeyNameToCode) {
    if (code == keycode) {
      std::string keyName = name;
      if (keyName.length() == 1) {
        keyName[0] = std::toupper(keyName[0]);
      }
      result += keyName;
      return result;
    }
  }

  // Fallback: character
  if (keycode >= 32 && keycode < 127) {
    result += static_cast<char>(keycode);
  } else {
    result += "?";
  }

  return result;
}

// -----------------------------------------------------------------------------
// ApplyFromSnapshot
// -----------------------------------------------------------------------------
void Keymap::ApplyFromSnapshot(const ConfigSnapshot &snapshot) {
  m_keys = snapshot.keys;

  // Rebuild chord_to_action map with proper parsing
  m_keys.chord_to_action.clear();

  for (const auto &[action_id, chords] : m_keys.action_to_chords) {
    for (const auto &chord : chords) {
      // Check for conflicts (last definition wins) [REQ-96]
      auto it = m_keys.chord_to_action.find(chord.packed);
      if (it != m_keys.chord_to_action.end()) {
        spdlog::warn(
            "[Keymap] Chord conflict: {} overwritten from '{}' to '{}'",
            FormatChord(chord.packed), it->second, action_id);
      }
      m_keys.chord_to_action[chord.packed] = action_id;
    }
  }

  spdlog::info("[Keymap] Applied {} action bindings",
               m_keys.action_to_chords.size());
}

// -----------------------------------------------------------------------------
// ActionForChord
// -----------------------------------------------------------------------------
std::string Keymap::ActionForChord(uint32_t chord_packed) const {
  auto it = m_keys.chord_to_action.find(chord_packed);
  return it != m_keys.chord_to_action.end() ? it->second : "";
}

// -----------------------------------------------------------------------------
// ChordsForAction
// -----------------------------------------------------------------------------
std::vector<Chord> Keymap::ChordsForAction(const std::string &action_id) const {
  auto it = m_keys.action_to_chords.find(action_id);
  return it != m_keys.action_to_chords.end() ? it->second
                                             : std::vector<Chord>{};
}

} // namespace arcanee::ide::config
