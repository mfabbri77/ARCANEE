/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file Time.cpp
 * @brief High-resolution timing implementation using SDL2.
 *
 * @ref specs/Appendix D §D.3.1
 *      "High-resolution monotonic timing."
 */

#include "Time.h"
#include <SDL.h>

namespace arcanee::platform {

double Time::now() {
  return static_cast<double>(SDL_GetPerformanceCounter()) /
         static_cast<double>(SDL_GetPerformanceFrequency());
}

u64 Time::ticks() { return SDL_GetPerformanceCounter(); }

u64 Time::tickFrequency() { return SDL_GetPerformanceFrequency(); }

double Time::ticksToSeconds(u64 tickCount) {
  return static_cast<double>(tickCount) /
         static_cast<double>(SDL_GetPerformanceFrequency());
}

u64 Time::secondsToTicks(double seconds) {
  return static_cast<u64>(seconds *
                          static_cast<double>(SDL_GetPerformanceFrequency()));
}

// Stopwatch implementation
Time::Stopwatch::Stopwatch() : m_startTicks(Time::ticks()) {}

void Time::Stopwatch::reset() { m_startTicks = Time::ticks(); }

double Time::Stopwatch::elapsed() const {
  return Time::ticksToSeconds(Time::ticks() - m_startTicks);
}

double Time::Stopwatch::elapsedMs() const { return elapsed() * 1000.0; }

double Time::Stopwatch::lap() {
  u64 now = Time::ticks();
  double delta = Time::ticksToSeconds(now - m_startTicks);
  m_startTicks = now;
  return delta;
}

// ScopedTimer implementation
Time::ScopedTimer::ScopedTimer(double *outElapsedMs)
    : m_outElapsedMs(outElapsedMs) {}

Time::ScopedTimer::~ScopedTimer() {
  if (m_outElapsedMs) {
    *m_outElapsedMs = m_stopwatch.elapsedMs();
  }
}

} // namespace arcanee::platform
