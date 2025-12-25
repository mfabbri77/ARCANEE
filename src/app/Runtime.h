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
 * @file Runtime.h
 * @brief ARCANEE main runtime orchestration.
 */

#include "audio/AudioManager.h"
#include "common/Types.h"
#include "input/InputManager.h"
#include "platform/Window.h"
#include "render/CBufPresets.h"
#include "render/Canvas2D.h"
#include "render/Framebuffer.h"
#include "render/PresentPass.h"
#include "render/RenderDevice.h"
#include "runtime/Cartridge.h"
#include "script/ScriptEngine.h"
#include "vfs/Vfs.h"
#include <memory>

namespace arcanee::app {
class Workbench; // Forward declaration

/**
 * @brief Main runtime class that orchestrates the ARCANEE engine.
 *
 * Manages:
 * - Main loop with fixed timestep (§2.3.3)
 * - Subsystem initialization/shutdown
 * - Cartridge lifecycle
 */
class Runtime {
public:
  struct Config {
    std::string cartridgePath;
    bool enableBenchmark = false;
    int benchmarkFrames = 600;
  };

  explicit Runtime(const Config &config);
  ~Runtime();

  int run();

  // Headless mode for testing/CI (MS-02)
  int runHeadless(int ticks);
  u64 getSimStateHash() const;

  // Cartridge management (separate load from run for IDE workflow)
  bool loadCartridge(const std::string &path);
  bool startCartridge();         // Start executing loaded cartridge
  void scheduleStartCartridge(); // Schedule start for next main loop iteration
                                 // (safe for UI callbacks)
  bool stopCartridge();          // Stop execution and reload (reset)
  bool isCartridgeLoaded() const;
  bool isCartridgeRunning() const;

  // Request application exit
  void requestExit() { m_isRunning = false; }

  input::InputManager *getInputManager() const { return m_inputManager.get(); }

  // Canvas2D accessor for IDE preview
  render::Canvas2D *getCanvas2D() const { return m_canvas2d.get(); }

  // ScriptEngine accessor for IDE debugger
  script::ScriptEngine *getScriptEngine() const { return m_scriptEngine.get(); }

  // Pause/resume for debugger synchronization
  void pause() { m_isPaused = true; }
  void resume() { m_isPaused = false; }
  bool isPaused() const { return m_isPaused; }

private:
  void initSubsystems();
  void shutdownSubsystems();

  void onDebugUpdate();

  void update(f64 dt);
  void draw(f64 alpha);

  bool m_isRunning;
  bool m_isHeadless = false;
  bool m_isBenchmark = false;
  bool m_isPaused = false;
  bool m_pendingStart = false;
  int m_benchmarkFrames = 0;

  // Subsystems
  std::unique_ptr<platform::Window> m_window;
  std::unique_ptr<vfs::IVfs> m_vfs;
  std::unique_ptr<script::ScriptEngine> m_scriptEngine;
  std::unique_ptr<render::RenderDevice> m_renderDevice;
  std::unique_ptr<audio::AudioManager> m_audioManager;
  std::unique_ptr<input::InputManager> m_inputManager;

  // Rendering (Phase 3.2-3.3)
  std::unique_ptr<render::Framebuffer> m_cbuf;
  std::unique_ptr<render::PresentPass> m_presentPass;
  std::unique_ptr<render::Canvas2D> m_canvas2d;
  render::CBufPreset m_cbufPreset = render::CBufPreset::Medium_16_9;

  std::unique_ptr<runtime::Cartridge> m_cartridge;
  std::string m_currentCartridgePath;
  std::vector<u32> m_palette;

  // Workbench (MS-04)
  friend class Workbench; // Access to private members? Or pass public API?
  // Ideally Runtime exposes public API for what Workbench needs.
  // For now, let's include header or fwd decl.
  // We need incomplete type support or include header.
  // Put include in .cpp, fwd decl here.
  std::unique_ptr<Workbench> m_workbench;
  bool m_showWorkbench = false;
};

} // namespace arcanee::app
