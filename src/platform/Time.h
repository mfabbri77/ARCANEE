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
 * @file Time.h
 * @brief High-resolution timing utilities.
 *
 * Provides monotonic timer access for the main loop and profiling.
 * All timing is based on SDL2's performance counter for cross-platform
 * consistency.
 *
 * @ref specs/Chapter 2 §2.3.3
 *      "Reference Implementation (Normative Pseudocode): The main loop
 *       uses high-resolution timing with fixed timestep updates."
 *
 * @ref specs/Appendix D §D.3.1
 *      "High-resolution monotonic timing."
 */

#include "common/Types.h"

namespace arcanee::platform {

/**
 * @brief High-resolution timer utilities.
 *
 * Provides access to SDL2's performance counter for precise timing.
 * All times are in seconds (double) or raw ticks (u64).
 */
class Time {
public:
  /**
   * @brief Get the current monotonic time in seconds.
   *
   * This time is relative to an arbitrary point (typically program start)
   * and should only be used for measuring durations.
   *
   * @return Current time in seconds (double precision).
   */
  static double now();

  /**
   * @brief Get the raw performance counter value.
   *
   * @return Raw tick count.
   */
  static u64 ticks();

  /**
   * @brief Get the performance counter frequency.
   *
   * @return Ticks per second.
   */
  static u64 tickFrequency();

  /**
   * @brief Convert ticks to seconds.
   *
   * @param tickCount Number of ticks.
   * @return Duration in seconds.
   */
  static double ticksToSeconds(u64 tickCount);

  /**
   * @brief Convert seconds to ticks.
   *
   * @param seconds Duration in seconds.
   * @return Number of ticks.
   */
  static u64 secondsToTicks(double seconds);

  /**
   * @brief Simple stopwatch for measuring elapsed time.
   */
  class Stopwatch {
  public:
    Stopwatch();

    /**
     * @brief Reset the stopwatch to now.
     */
    void reset();

    /**
     * @brief Get elapsed time in seconds since last reset.
     */
    double elapsed() const;

    /**
     * @brief Get elapsed time in milliseconds since last reset.
     */
    double elapsedMs() const;

    /**
     * @brief Get elapsed time and reset the stopwatch.
     */
    double lap();

  private:
    u64 m_startTicks;
  };

  /**
   * @brief Scoped timer for automatic profiling.
   *
   * Records elapsed time when the scope ends.
   */
  class ScopedTimer {
  public:
    /**
     * @brief Create a scoped timer.
     *
     * @param outElapsedMs Pointer to receive elapsed time in ms.
     */
    explicit ScopedTimer(double *outElapsedMs);
    ~ScopedTimer();

    // Non-copyable
    ScopedTimer(const ScopedTimer &) = delete;
    ScopedTimer &operator=(const ScopedTimer &) = delete;

  private:
    Stopwatch m_stopwatch;
    double *m_outElapsedMs;
  };
};

} // namespace arcanee::platform
