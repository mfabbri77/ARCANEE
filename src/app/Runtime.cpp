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
 *
 * Implements the fixed timestep main loop as specified in Chapter 2.
 *
 * @ref specs/Chapter 2 §2.3.3
 *      "Reference Implementation (Normative Pseudocode)"
 */

#include "Runtime.h"
#include "common/Log.h"
#include "platform/Time.h"
#include <algorithm>

namespace arcanee::app {

// ============================================================================
// Normative Constants from Chapter 2 §2.3.3
// ============================================================================

/**
 * @brief Simulation tick rate.
 * @ref specs/Chapter 2 §2.3.3: "const double dt_fixed = 1.0 / 60.0"
 */
constexpr double kTickHz = 60.0;
constexpr double kDtFixed = 1.0 / kTickHz;

/**
 * @brief Maximum updates per frame to prevent spiral of death.
 * @ref specs/Chapter 2 §2.3.3: "const int max_updates = 4"
 */
constexpr int kMaxUpdatesPerFrame = 4;

/**
 * @brief Maximum frame time clamp (prevents huge deltas during debugging).
 */
constexpr double kMaxFrameTime = 0.25;

// ============================================================================
// Implementation
// ============================================================================

Runtime::Runtime() { InitSubsystems(); }

Runtime::~Runtime() { ShutdownSubsystems(); }

void Runtime::InitSubsystems() {
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
    m_running = false;
    return;
  }

  LOG_INFO("Runtime: Subsystems initialized");
}

void Runtime::ShutdownSubsystems() {
  LOG_INFO("Runtime: Shutting down subsystems");

  // Destroy in reverse order
  m_window.reset();

  LOG_INFO("Runtime: Subsystems shutdown complete");
}

int Runtime::Run() {
  if (!m_running) {
    LOG_ERROR("Runtime: Cannot run, initialization failed");
    return 1;
  }

  LOG_INFO("Runtime: Starting main loop (Fixed Timestep: %.0f Hz)", kTickHz);

  // ============================================================================
  // Main Loop - Fixed Timestep per Chapter 2 §2.3.3
  // ============================================================================

  double accumulator = 0.0;
  platform::Time::Stopwatch frameTimer;

  while (m_running && !m_window->shouldClose()) {
    // 1. Timing
    double frameTime = frameTimer.lap();

    // Clamp large frame times (e.g., during debugging or window drag)
    // @ref specs/Chapter 2 §2.3.3: Bounded catch-up
    if (frameTime > kMaxFrameTime) {
      LOG_DEBUG("Runtime: Frame time clamped from %.2fms to %.2fms",
                frameTime * 1000.0, kMaxFrameTime * 1000.0);
      frameTime = kMaxFrameTime;
    }

    accumulator += frameTime;

    // 2. Event Pump
    // @ref specs/Chapter 2 §2.3: "Phase 1 – Event Pump"
    m_window->pollEvents();

    // Handle window close
    if (m_window->shouldClose()) {
      m_running = false;
      break;
    }

    // Handle window resize (future: recreate swapchain)
    if (m_window->wasResized()) {
      int w = 0, h = 0;
      m_window->getDrawableSize(w, h);
      LOG_DEBUG("Runtime: Window resized to %dx%d", w, h);
      // TODO: Notify renderer of resize
    }

    // 3. Fixed Updates
    // @ref specs/Chapter 2 §2.3.3:
    //   "while (accumulator >= dt_fixed && update_count < max_updates)"
    int updateCount = 0;
    while (accumulator >= kDtFixed && updateCount < kMaxUpdatesPerFrame) {
      Update(kDtFixed);
      accumulator -= kDtFixed;
      updateCount++;
    }

    // Spiral of death prevention
    // @ref specs/Chapter 2 §2.3.3: "If the real time debt ever exceeds
    // max_updates × dt_fixed, the engine drops accumulated time."
    if (accumulator > kDtFixed * kMaxUpdatesPerFrame) {
      LOG_WARN("Runtime: Dropping accumulated time (spiral of death "
               "prevention)");
      accumulator = 0.0;
    }

    // 4. Draw
    // @ref specs/Chapter 2 §2.3.3: "double alpha = accumulator / dt_fixed;"
    double alpha = accumulator / kDtFixed;
    alpha = std::clamp(alpha, 0.0, 1.0);
    Draw(alpha);

    // Sleep if frame was very fast (avoid 100% CPU on empty loop)
    // In production: VSync or renderer waiting handles this
    if (frameTime < 0.001 && !m_window->isMinimized()) {
      // Minimal sleep to yield CPU
      SDL_Delay(1);
    }
  }

  LOG_INFO("Runtime: Main loop ended");
  return 0;
}

void Runtime::Update(double dt) {
  (void)dt;
  // TODO: Cartridge update callback
  // @ref specs/Chapter 4 §4.4.2: "function update(dt)"
}

void Runtime::Draw(double alpha) {
  (void)alpha;
  // TODO: Render pass execution
  // @ref specs/Chapter 5 §5.5: "Render Pass Order"
  //   1. 3D Scene Pass (if scene3d.enable())
  //   2. 2D Canvas Pass
  //   3. Present Pass (scale CBUF to backbuffer)
  //   4. UI Overlay Pass (Workbench only)
}

} // namespace arcanee::app
