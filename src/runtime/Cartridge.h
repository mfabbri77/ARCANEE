#pragma once

/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file Cartridge.h
 */

#include <string>

namespace arcanee::runtime {

// Cartridge state machine per Chapter 4 §4.2.1
enum class CartridgeState {
  Unloaded,    // No VM instance exists
  Loading,     // VFS mounted; VM creation in progress
  Initialized, // init() executed successfully
  Running,     // update/draw executing
  Paused,      // Execution suspended
  Faulted,     // Unrecoverable error occurred
  Stopped      // Intentionally stopped by user
};

const char *cartridgeStateToString(CartridgeState state);

// Cartridge configuration from manifest (Chapter 3 §3.4)
struct CartridgeConfig {
  // Required fields
  std::string id;
  std::string title;
  std::string version;
  std::string apiVersion;
  std::string entry; // Entry script path (e.g., "main.nut")

  // Display settings (§3.4.2)
  struct Display {
    enum class Aspect { Ratio16x9, Ratio4x3, Any };
    enum class Preset { Low, Medium, High, Ultra };
    enum class Scaling { Fit, IntegerNearest, Fill, Stretch };

    Aspect aspect = Aspect::Ratio16x9;
    Preset preset = Preset::Medium;
    Scaling scaling = Scaling::Fit;
    bool allowUserOverride = true;
  } display;

  // Permissions (§3.4.3)
  struct Permissions {
    bool saveStorage = true;
    bool audio = true;
    bool net = false;
    bool native = false;
  } permissions;

  // Caps (§3.4.4) - advisory hints
  struct Caps {
    float cpuMsPerUpdate = 2.0f;
    int vmMemoryMb = 64;
    int maxDrawCalls = 20000;
    int maxCanvasPixels = 16777216;
    int audioChannels = 32;
  } caps;
};

// Get CBUF dimensions for a preset (Chapter 5 §5.3.1)
void getCbufDimensions(CartridgeConfig::Display::Aspect aspect,
                       CartridgeConfig::Display::Preset preset, int &outWidth,
                       int &outHeight);

} // namespace arcanee::runtime
