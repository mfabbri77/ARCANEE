/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file Runtime.cpp
 * @brief ARCANEE main runtime implementation.
 */

#include "Runtime.h"
#include "common/Log.h"
#include "platform/Time.h"
#include "render/RenderDevice.h"
#include <SDL2/SDL.h>
#include <algorithm>

namespace arcanee::app {

// ============================================================================
// Normative Constants from Chapter 2 §2.3.3
// ============================================================================

constexpr double kTickHz = 60.0;
constexpr double kDtFixed = 1.0 / kTickHz;
constexpr int kMaxUpdatesPerFrame = 4;
constexpr double kMaxFrameTime = 0.25;

// ============================================================================
// Implementation
// ============================================================================

Runtime::Runtime() { initSubsystems(); }

Runtime::~Runtime() { shutdownSubsystems(); }

void Runtime::initSubsystems() {
  LOG_INFO("Runtime: Initializing subsystems");

  // 1. Create Window
  platform::WindowConfig winConfig;
  winConfig.title = "ARCANEE v0.1";
  winConfig.width = 1280;
  winConfig.height = 720;
  winConfig.resizable = true;
  winConfig.highDpi = true;

  m_window = std::make_unique<platform::Window>(winConfig);

  if (!m_window->isOpen()) {
    LOG_ERROR("Runtime: Failed to create window");
    m_isRunning = false;
    return;
  }

  // 2. Initialize VFS
  m_vfs = vfs::createVfs();

  // 3. Initialize Script Engine (Moved to step 6 for flow but keeping ptr)
  // m_scriptEngine is initialized later

  // 4. Create Cartridge Manager (Placeholder removed, real load is later)

  // 5. Initialize Rendering (Phase 3)
  m_renderDevice = std::make_unique<render::RenderDevice>();
  if (!m_renderDevice->initialize(m_window->getNativeHandle())) {
    LOG_ERROR("Failed to initialize RenderDevice");
  }

  // 6. Initialize Script Engine
  m_scriptEngine = std::make_unique<script::ScriptEngine>();

  // 7. Load Cartridge
  // m_vfs is a unique_ptr, so pass raw pointer via .get()
  m_cartridge =
      std::make_unique<runtime::Cartridge>(m_vfs.get(), m_scriptEngine.get());
  if (!m_cartridge->load("samples/hello")) {
    LOG_ERROR("Failed to load cartridge");
  } else {
    LOG_INFO("Cartridge State: Unloaded -> Loading");
  }

  m_isRunning = true;
  LOG_INFO("Runtime: Subsystems initialized");
}

void Runtime::shutdownSubsystems() {
  LOG_INFO("Runtime: Shutting down subsystems");

  // Unload cartridge first
  if (m_cartridge) {
    m_cartridge->unload();
    m_cartridge.reset();
  }

  m_scriptEngine.reset(); // Correct for unique_ptr
  m_renderDevice.reset();

  // VFS depends on PhysFS, safe to destroy last
  m_vfs.reset();

  m_window.reset();

  LOG_INFO("Runtime: Subsystems shutdown complete");
}

int Runtime::run() {
  if (!m_isRunning) {
    LOG_ERROR("Runtime: Cannot run, initialization failed");
    return 1;
  }

  LOG_INFO("Runtime: Starting main loop (Fixed Timestep: %.0f Hz)", kTickHz);

  double accumulator = 0.0;
  platform::Time::Stopwatch frameTimer;

  while (m_isRunning && !m_window->shouldClose()) {
    // 1. Timing
    double frameTime = frameTimer.lap();
    if (frameTime > kMaxFrameTime)
      frameTime = kMaxFrameTime;
    accumulator += frameTime;

    // 2. Event Pump
    m_window->pollEvents();
    if (m_window->shouldClose()) {
      m_isRunning = false;
      break;
    }

    if (m_window->wasResized()) {
      int w = 0, h = 0;
      m_window->getDrawableSize(w, h);
      LOG_DEBUG("Runtime: Window resized to %dx%d", w, h);
    }

    // 3. Fixed Updates
    int updateCount = 0;
    while (accumulator >= kDtFixed && updateCount < kMaxUpdatesPerFrame) {
      update(kDtFixed);
      accumulator -= kDtFixed;
      updateCount++;
    }

    if (accumulator > kDtFixed * kMaxUpdatesPerFrame) {
      accumulator = 0.0;
    }

    // 4. Draw
    double alpha = accumulator / kDtFixed;
    alpha = std::clamp(alpha, 0.0, 1.0);
    draw(alpha);

    // Sleep
    if (frameTime < 0.001 && !m_window->isMinimized()) {
      SDL_Delay(1);
    }
  }

  LOG_INFO("Runtime: Main loop ended");
  return 0;
}

void Runtime::update(f64 dt) {
  if (m_cartridge) {
    m_cartridge->update(dt);
  }
}

void Runtime::draw(f64 alpha) {
  if (m_cartridge) {
    m_cartridge->draw(alpha);
  }
}

} // namespace arcanee::app
