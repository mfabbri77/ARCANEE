/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file Window.cpp
 * @brief SDL2 Window implementation with fullscreen and DPI support.
 *
 * @ref specs/Chapter 2 §2.1
 *      Platform layer responsibilities.
 *
 * @ref specs/Chapter 5 §5.12
 *      "Fullscreen Policy: The runtime SHOULD support borderless fullscreen
 *       mode (SDL_WINDOW_FULLSCREEN_DESKTOP) rather than exclusive fullscreen."
 */

#include "Window.h"
#include "common/Log.h"

namespace arcanee::platform {

Window::Window(const WindowConfig &config)
    : m_fullscreenMode(config.fullscreen) {

  // Build window flags
  Uint32 flags = SDL_WINDOW_SHOWN;

  if (config.resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }

  if (config.highDpi) {
    flags |= SDL_WINDOW_ALLOW_HIGHDPI;
  }

  // Initial fullscreen mode
  switch (config.fullscreen) {
  case FullscreenMode::FullscreenDesktop:
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    break;
  case FullscreenMode::FullscreenExclusive:
    flags |= SDL_WINDOW_FULLSCREEN;
    break;
  case FullscreenMode::Windowed:
    break;
  }

  // Determine window position
  int displayIndex = (config.displayIndex >= 0) ? config.displayIndex : 0;
  int posX = SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex);
  int posY = SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex);

  // Create the window
  m_window = SDL_CreateWindow(config.title.c_str(), posX, posY, config.width,
                              config.height, flags);

  if (!m_window) {
    LOG_ERROR("Window: Failed to create SDL window: %s", SDL_GetError());
    return;
  }

  // Store initial windowed geometry for later restoration
  m_windowedW = config.width;
  m_windowedH = config.height;
  SDL_GetWindowPosition(m_window, &m_windowedX, &m_windowedY);

  // Log creation info
  int w = 0, h = 0;
  getDrawableSize(w, h);
  LOG_INFO("Window: Created '%s' (%dx%d drawable) on display %d",
           config.title.c_str(), w, h, getDisplayIndex());
}

Window::~Window() {
  if (m_window) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
    LOG_DEBUG("Window: Destroyed");
  }
}

void Window::pollEvents() {
  // Clear per-frame flags
  m_wasResized = false;

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      m_shouldClose = true;
      break;

    case SDL_WINDOWEVENT:
      if (event.window.windowID == SDL_GetWindowID(m_window)) {
        switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
          m_shouldClose = true;
          break;

        case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
          m_wasResized = true;
          LOG_DEBUG("Window: Resized to %dx%d", event.window.data1,
                    event.window.data2);
          break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
          m_hasKeyboardFocus = true;
          LOG_DEBUG("Window: Keyboard focus gained");
          break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
          m_hasKeyboardFocus = false;
          LOG_DEBUG("Window: Keyboard focus lost");
          break;

        case SDL_WINDOWEVENT_ENTER:
          m_hasMouseFocus = true;
          break;

        case SDL_WINDOWEVENT_LEAVE:
          m_hasMouseFocus = false;
          break;

        case SDL_WINDOWEVENT_MINIMIZED:
          m_isMinimized = true;
          LOG_DEBUG("Window: Minimized");
          break;

        case SDL_WINDOWEVENT_RESTORED:
          m_isMinimized = false;
          LOG_DEBUG("Window: Restored");
          break;
        }
      }
      break;
    }
  }
}

void Window::getSize(int &w, int &h) const {
  if (m_window) {
    SDL_GetWindowSize(m_window, &w, &h);
  } else {
    w = 0;
    h = 0;
  }
}

void Window::getDrawableSize(int &w, int &h) const {
  if (!m_window) {
    w = 0;
    h = 0;
    return;
  }

  // Try to get actual drawable size (for high-DPI)
  // SDL_GL_GetDrawableSize requires OpenGL context, so we check the renderer
  // For now, use window size as fallback
  // TODO: When renderer is integrated, use proper drawable size query

  SDL_GetWindowSize(m_window, &w, &h);

  // Note: With SDL_WINDOW_ALLOW_HIGHDPI, drawable size may differ from window
  // size.  This will be properly handled when we integrate with DiligentEngine
  // or use SDL_Renderer.
}

int Window::getDisplayIndex() const {
  if (!m_window) {
    return -1;
  }
  return SDL_GetWindowDisplayIndex(m_window);
}

bool Window::setFullscreenMode(FullscreenMode mode) {
  if (!m_window) {
    return false;
  }

  // Save windowed geometry before going fullscreen
  if (m_fullscreenMode == FullscreenMode::Windowed &&
      mode != FullscreenMode::Windowed) {
    SDL_GetWindowPosition(m_window, &m_windowedX, &m_windowedY);
    SDL_GetWindowSize(m_window, &m_windowedW, &m_windowedH);
  }

  Uint32 flags = 0;
  switch (mode) {
  case FullscreenMode::Windowed:
    flags = 0;
    break;
  case FullscreenMode::FullscreenDesktop:
    flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
    break;
  case FullscreenMode::FullscreenExclusive:
    flags = SDL_WINDOW_FULLSCREEN;
    break;
  }

  if (SDL_SetWindowFullscreen(m_window, flags) != 0) {
    LOG_ERROR("Window: Failed to set fullscreen mode: %s", SDL_GetError());
    return false;
  }

  m_fullscreenMode = mode;

  // Restore windowed geometry when leaving fullscreen
  if (mode == FullscreenMode::Windowed) {
    SDL_SetWindowPosition(m_window, m_windowedX, m_windowedY);
    SDL_SetWindowSize(m_window, m_windowedW, m_windowedH);
  }

  const char *modeStr = "Windowed";
  if (mode == FullscreenMode::FullscreenDesktop) {
    modeStr = "Fullscreen Desktop";
  } else if (mode == FullscreenMode::FullscreenExclusive) {
    modeStr = "Fullscreen Exclusive";
  }
  LOG_INFO("Window: Fullscreen mode set to %s", modeStr);

  return true;
}

bool Window::toggleFullscreen() {
  if (m_fullscreenMode == FullscreenMode::Windowed) {
    // Switch to borderless fullscreen (recommended per spec)
    return setFullscreenMode(FullscreenMode::FullscreenDesktop);
  } else {
    return setFullscreenMode(FullscreenMode::Windowed);
  }
}

} // namespace arcanee::platform
