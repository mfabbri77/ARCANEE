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
#include "script/api/AudioBinding.h"
#include "script/api/GfxBinding.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

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

Runtime::Runtime(const std::string &cartridgePath, bool headless)
    : m_isHeadless(headless) {
  initSubsystems();
  loadCartridge(cartridgePath);
}

Runtime::~Runtime() {
  script::setAudioVfs(nullptr); // Added
  audio::setAudioManager(nullptr);
  shutdownSubsystems();
}

void Runtime::initSubsystems() {
  LOG_INFO("Runtime: Initializing subsystems");

  // 1. Create Window
  platform::WindowConfig winConfig;
  winConfig.title = "ARCANEE v0.1";
  winConfig.width = 1280;
  winConfig.height = 720;
  winConfig.resizable = true;
  winConfig.highDpi = true;

  if (!m_isHeadless) {
    m_window = std::make_unique<platform::Window>(winConfig);

    if (!m_window->isOpen()) {
      LOG_ERROR("Runtime: Failed to create window");
      m_isRunning = false;
      return;
    }
  } else {
    // In headless, we skip Window creation.
    // If InputManager needs it, we pass nullptr.
    LOG_INFO("Runtime: Running in HEADLESS mode - Window creation skipped");
  }

  // This check is now only relevant if m_window was actually created.
  // If m_isHeadless is true, m_window is nullptr, and this check would crash.
  // The original code had a similar issue, but `m_window` was always created
  // and then reset in headless mode.
  // To be safe, we should only check if m_window exists.
  if (m_window && !m_window->isOpen()) {
    LOG_ERROR("Runtime: Failed to create window");
    m_isRunning = false;
    return;
  }

  // 1b. Initialize Input
  m_inputManager = std::make_unique<input::InputManager>();
  if (!m_inputManager->initialize(m_window.get())) {
    LOG_ERROR("Failed to initialize InputManager");
    // Non-fatal, but problematic
  }
  input::setInputManager(m_inputManager.get());

  // 2. Initialize VFS
  m_vfs = vfs::createVfs();

  // 2b. Initialize Audio
  m_audioManager = std::make_unique<audio::AudioManager>();
  if (!m_audioManager->initialize()) {
    LOG_ERROR("Failed to initialize AudioManager");
    // Audio failure is non-fatal for now
  }
  audio::setAudioManager(m_audioManager.get());
  script::setAudioVfs(m_vfs.get()); // Added

  // 3. Initialize Script Engine (Moved to step 6 for flow but keeping ptr)
  // m_scriptEngine is initialized later

  // 4. Create Cartridge Manager (Placeholder removed, real load is later)

  // 5. Initialize Rendering (Phase 3)
  if (!m_isHeadless) {
    m_renderDevice = std::make_unique<render::RenderDevice>();
    auto windowInfo = m_window->getNativeWindowInfo();
    if (!m_renderDevice->initialize(windowInfo.display, windowInfo.window)) {
      LOG_ERROR("Failed to initialize RenderDevice");
      m_isRunning = false;
      return;
    }
  }

  // 5b. Create CBUF (Phase 3.2)
  if (!m_isHeadless) {
    auto cbufDims = render::getCBufDimensions(m_cbufPreset);
    m_cbuf = std::make_unique<render::Framebuffer>();
    if (!m_cbuf->create(*m_renderDevice, cbufDims.width, cbufDims.height,
                        true)) {
      LOG_ERROR("Failed to create CBUF");
      m_isRunning = false;
      return;
    }
    LOG_INFO("CBUF: %ux%u (%s)", cbufDims.width, cbufDims.height,
             render::getCBufAspectString(m_cbufPreset));

    // 5c. Create Present Pass (Phase 3.3)
    m_presentPass = std::make_unique<render::PresentPass>();
    if (!m_presentPass->initialize(*m_renderDevice)) {
      LOG_ERROR("Failed to initialize PresentPass");
      m_isRunning = false;
      return;
    }

    // 5d. Create Canvas2D (Phase 4.1 - ThorVG)
    m_canvas2d = std::make_unique<render::Canvas2D>();
    if (!m_canvas2d->initialize(*m_renderDevice, cbufDims.width,
                                cbufDims.height)) {
      LOG_ERROR("Failed to initialize Canvas2D");
      m_isRunning = false;
      return;
    }

    // 5e. Initialize Palette (PICO-8 Standard)
    m_palette.clear();
    m_palette.push_back(0x00000000); // 0: Transparent
    m_palette.push_back(0xFF1D2B53); // 1: Dark Blue
    m_palette.push_back(0xFF7E2553); // 2: Dark Purple
    m_palette.push_back(0xFF008751); // 3: Dark Green
    m_palette.push_back(0xFFAB5236); // 4: Brown
    m_palette.push_back(0xFF5F574F); // 5: Dark Gray
    m_palette.push_back(0xFFC2C3C7); // 6: Light Gray
    m_palette.push_back(0xFFFFF1E8); // 7: White
    m_palette.push_back(0xFFFF004D); // 8: Red
    m_palette.push_back(0xFFFFA300); // 9: Orange
    m_palette.push_back(0xFFFFEC27); // 10: Yellow
    m_palette.push_back(0xFF00E436); // 11: Green
    m_palette.push_back(0xFF29ADFF); // 12: Blue
    m_palette.push_back(0xFF83769C); // 13: Indigo
    m_palette.push_back(0xFFFF77A8); // 14: Pink
    m_palette.push_back(0xFFFFCCAA); // 15: Peach

    arcanee::script::setGfxPalette(&m_palette);
    arcanee::script::setGfxCanvas(m_canvas2d.get());
  } else {
    // In headless, we don't have Canvas2D.
    // Ensure scripts don't crash if they try to draw.
    // setGfxCanvas(nullptr) is default. GfxBinding should check for null.
    LOG_INFO("Runtime: Running in HEADLESS mode - Rendering skipped");
  }

  // 6. Initialize Script Engine
  m_scriptEngine = std::make_unique<script::ScriptEngine>();

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

  m_scriptEngine.reset();
  m_presentPass.reset();
  m_cbuf.reset();
  m_renderDevice.reset();

  // VFS depends on PhysFS, safe to destroy last
  m_vfs.reset();

  m_window.reset();

  input::setInputManager(nullptr);
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

int Runtime::runHeadless(int ticks) {
  if (!m_isRunning)
    return 1;
  LOG_INFO("Runtime: Running HEADLESS for %d ticks", ticks);

  for (int i = 0; i < ticks; ++i) {
    if (m_inputManager)
      m_inputManager->update();
    update(kDtFixed);
  }
  return 0;
}

u64 Runtime::getSimStateHash() const {
  // Simplistic hash: Tick Count + Input State
  // Ideally this hashes VM memory (stack/heap).
  u64 hash = 0;
  if (m_inputManager) {
    const auto &snap = m_inputManager->getCurrentSnapshot();
    // Hash some key inputs
    hash ^= (u64)snap.mouse.x;
    hash ^= (u64)snap.mouse.y << 16;
    // Hash keys? Too large. Just a few for verification.
    if (snap.keys[SDL_SCANCODE_SPACE])
      hash ^= 0xCAFEBABE;
  }
  return hash;
}

void Runtime::update(f64 dt) {
  if (m_cartridge) {
    m_cartridge->update(dt);
  }
}

void Runtime::draw(f64 alpha) {
  // 1. Clear CBUF to black (letterbox color)
  if (m_cbuf && m_renderDevice) {
    m_cbuf->clear(m_renderDevice->getContext(), 0.0f, 0.0f, 0.0f, 1.0f);
  }

  // 2. Canvas2D rendering (ThorVG)
  if (m_canvas2d && m_canvas2d->isValid()) {
    m_canvas2d->beginFrame();

    // Clear canvas to dark blue background? Or let cartridge do it?
    // Spec says cartridge 'gfx.clear' handles it. But we should probably clear
    // to transparent or black initially? Let's clear to transparent so CBUF
    // background shows if cartridge doesn't clear.
    // m_canvas2d->clear(0x00000000);

    // 3. Run cartridge draw (Generates canvas commands)
    if (m_cartridge) {
      m_cartridge->draw(alpha);
    }

    m_canvas2d->endFrame(*m_renderDevice);
  }

  // 4. Present Canvas2D to backbuffer (through CBUF)
  // For now we present the Canvas2D texture directly
  if (m_presentPass && m_canvas2d && m_canvas2d->isValid()) {
    m_presentPass->execute(*m_renderDevice, m_canvas2d->getShaderResourceView(),
                           m_canvas2d->getWidth(), m_canvas2d->getHeight(),
                           render::PresentMode::Fit);
  }

  // 5. Present swapchain
  if (m_renderDevice) {
    m_renderDevice->present();
  }
}

// ----------------------------------------------------------------------------
// Cartridge Loading
// ----------------------------------------------------------------------------

bool Runtime::loadCartridge(const std::string &path) {
  LOG_INFO("Runtime: Loading cartridge from '%s'", path.c_str());

  // 1. Create Cartridge Manager
  // m_vfs is a unique_ptr, so pass raw pointer via .get()
  m_cartridge =
      std::make_unique<runtime::Cartridge>(m_vfs.get(), m_scriptEngine.get());

  // 2. Load Cartridge (handles VFS mount and script load)
  if (!m_cartridge->load(path)) {
    LOG_ERROR("Runtime: Failed to load cartridge");
    return false;
  }

  LOG_INFO("Runtime: Cartridge loaded successfully");
  return true;
}

} // namespace arcanee::app
