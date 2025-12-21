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
 *
 * The Runtime class manages the main loop with fixed timestep updates,
 * subsystem initialization, and cartridge lifecycle.
 *
 * @ref specs/Chapter 2 §2.3.3
 *      "Reference Implementation (Normative Pseudocode)"
 */

#include "platform/Window.h"
#include <memory>

namespace arcanee::app {

/**
 * @brief Main runtime class that orchestrates the ARCANEE engine.
 *
 * Manages:
 * - Main loop with fixed timestep (§2.3.3)
 * - Subsystem initialization/shutdown
 * - Cartridge lifecycle (future)
 */
class Runtime {
public:
  Runtime();
  ~Runtime();

  /**
   * @brief Run the main loop until exit.
   *
   * @return Exit code (0 = success).
   *
   * @ref specs/Chapter 2 §2.3
   *      "Main loop phases: Event Pump → Input Snapshot → Fixed Update →
   * Draw."
   */
  int Run();

private:
  void InitSubsystems();
  void ShutdownSubsystems();

  /**
   * @brief Fixed timestep update.
   *
   * @param dt Fixed delta time in seconds.
   *
   * @ref specs/Chapter 2 §2.3.3
   *      "const double dt_fixed = 1.0 / 60.0; // 16.67ms"
   */
  void Update(double dt);

  /**
   * @brief Draw/render frame.
   *
   * @param alpha Interpolation factor for smooth rendering.
   *
   * @ref specs/Chapter 2 §2.3.3
   *      "double alpha = accumulator / dt_fixed;"
   */
  void Draw(double alpha);

  // Subsystems
  std::unique_ptr<platform::Window> m_window;

  // State
  bool m_running = true;
};

} // namespace arcanee::app
