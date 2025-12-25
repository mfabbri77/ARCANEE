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

#include "script/ScriptEngine.h"
#include "vfs/Vfs.h"
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
  std::string entry = "main.nut"; // Entry script path (default: main.nut)

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

/**
 * @brief Manages the lifecycle of a single cartridge instance.
 */
class Cartridge {
public:
  Cartridge(vfs::IVfs *vfs, script::ScriptEngine *engine);
  ~Cartridge();

  /**
   * @brief Load a cartridge from the given path (directory or archive).
   *
   * Only mounts VFS - does NOT execute scripts.
   * Transitions: Unloaded -> Loading -> Initialized (on success) or Faulted.
   */
  bool load(const std::string &fsPath);

  /**
   * @brief Start executing the loaded cartridge.
   *
   * Executes the entry script and calls init().
   * Call this after load() to begin running.
   * Transitions: Initialized -> Running.
   */
  bool start();

  /**
   * @brief Unload the current cartridge.
   *
   * Transitions: Any -> Unloaded.
   */
  void unload();

  /**
   * @brief Update the cartridge state.
   *
   * Should be called once per fixed timestep.
   * Only calls script update() if in Running state.
   */
  void update(double dt);

  /**
   * @brief Draw the cartridge content.
   *
   * Should be called once per frame.
   * Only calls script draw() if in Running state (or Paused if allowed).
   */
  void draw(double alpha);

  CartridgeState getState() const { return m_state; }

  // Get the loaded entry script path for debugger
  std::string getEntryPath() const { return "cart:/" + m_config.entry; }

private:
  void transition(CartridgeState newState);

  vfs::IVfs *m_vfs;
  script::ScriptEngine *m_scriptEngine;

  CartridgeState m_state = CartridgeState::Unloaded;
  CartridgeConfig m_config;
};

} // namespace arcanee::runtime
