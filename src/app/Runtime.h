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

#include "platform/Window.h"
#include "render/CBufPresets.h"
#include "render/Framebuffer.h"
#include "render/PresentPass.h"
#include "render/RenderDevice.h"
#include "runtime/Cartridge.h"
#include "script/ScriptEngine.h"
#include "vfs/Vfs.h"
#include <memory>

namespace arcanee::app {

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
  Runtime();
  ~Runtime();

  int run();

private:
  void initSubsystems();
  void shutdownSubsystems();

  void update(f64 dt);
  void draw(f64 alpha);

  bool m_isRunning;

  // Subsystems
  std::unique_ptr<platform::Window> m_window;
  std::unique_ptr<vfs::IVfs> m_vfs;
  std::unique_ptr<script::ScriptEngine> m_scriptEngine;
  std::unique_ptr<render::RenderDevice> m_renderDevice;

  // Rendering (Phase 3.2-3.3)
  std::unique_ptr<render::Framebuffer> m_cbuf;
  std::unique_ptr<render::PresentPass> m_presentPass;
  render::CBufPreset m_cbufPreset = render::CBufPreset::Medium_16_9;

  std::unique_ptr<runtime::Cartridge> m_cartridge;
};

} // namespace arcanee::app
