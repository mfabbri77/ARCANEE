// ARCANEE Keymap - [API-21]
// Parse and apply keybindings with deterministic conflict resolution.
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#pragma once
#include "ConfigSnapshot.h"
#include <string>
#include <vector>

namespace arcanee::ide::config {

// -----------------------------------------------------------------------------
// Modifier flags for chord normalization
// -----------------------------------------------------------------------------
enum class ModifierFlags : uint8_t {
  None = 0,
  Ctrl = 1 << 0,
  Alt = 1 << 1,
  Shift = 1 << 2,
  Super = 1 << 3, // Win key / Cmd key
};

inline ModifierFlags operator|(ModifierFlags a, ModifierFlags b) {
  return static_cast<ModifierFlags>(static_cast<uint8_t>(a) |
                                    static_cast<uint8_t>(b));
}

inline ModifierFlags operator&(ModifierFlags a, ModifierFlags b) {
  return static_cast<ModifierFlags>(static_cast<uint8_t>(a) &
                                    static_cast<uint8_t>(b));
}

inline bool HasMod(ModifierFlags flags, ModifierFlags test) {
  return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
}

// -----------------------------------------------------------------------------
// Keymap - keybinding management [REQ-96]
// -----------------------------------------------------------------------------
class Keymap {
public:
  Keymap();
  ~Keymap();

  // Apply keybindings from config snapshot
  void ApplyFromSnapshot(const ConfigSnapshot &snapshot);

  // Lookup action for a chord (returns empty string if not bound)
  std::string ActionForChord(uint32_t chord_packed) const;

  // Get all chords bound to an action (for UI display)
  std::vector<Chord> ChordsForAction(const std::string &action_id) const;

  // Parse a chord string (e.g., "primary+S", "Ctrl+Shift+P")
  // Returns packed chord value, or 0 if invalid
  static uint32_t ParseChordString(const std::string &chord_str);

  // Format a chord for display
  static std::string FormatChord(uint32_t chord_packed);

  // Check if current platform is macOS (for `primary` expansion)
  static bool IsMacOS();

private:
  KeysConfig m_keys;
};

} // namespace arcanee::ide::config
