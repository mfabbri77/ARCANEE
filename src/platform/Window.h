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
 * @file Window.h
 * @brief SDL2 Window management with fullscreen and DPI support.
 *
 * @ref specs/Chapter 2 §2.1
 *      "Platform Layer (SDL2): windowing, input, high-resolution timing,
 *       display enumeration, fullscreen control."
 *
 * @ref specs/Chapter 5 §5.12
 *      "Fullscreen Policy: The runtime SHOULD support borderless fullscreen
 *       mode (SDL_WINDOW_FULLSCREEN_DESKTOP) rather than exclusive fullscreen,
 *       to avoid mode changes and ensure Alt+Tab performance."
 */

#include <SDL.h>
#include <string>

namespace arcanee::platform {

/**
 * @brief Window fullscreen mode.
 *
 * @ref specs/Chapter 5 §5.12
 */
enum class FullscreenMode {
  Windowed,           ///< Normal windowed mode
  FullscreenDesktop,  ///< Borderless fullscreen (recommended)
  FullscreenExclusive ///< Exclusive fullscreen (mode change)
};

/**
 * @brief Window configuration.
 */
struct WindowConfig {
  std::string title = "ARCANEE";
  int width = 1280;      ///< Initial window width
  int height = 720;      ///< Initial window height
  bool resizable = true; ///< Allow window resizing
  bool highDpi = true;   ///< Enable high-DPI rendering
  int displayIndex = -1; ///< Target display (-1 = primary)
  FullscreenMode fullscreen = FullscreenMode::Windowed;
};

/**
 * @brief SDL2 Window wrapper with fullscreen, resize, and DPI support.
 *
 * The Window class manages an SDL2 window and provides:
 * - Fullscreen mode switching (borderless desktop recommended)
 * - DPI-aware drawable size queries
 * - Event polling with focus tracking
 *
 * @ref specs/Chapter 2 §2.1
 *      Platform layer windowing responsibilities.
 *
 * @ref specs/Chapter 5 §5.12.1
 *      "In Workbench mode, fullscreen desktop MUST apply to the currently
 *       selected display."
 */
class Window {
public:
  /**
   * @brief Create a new window.
   *
   * @param config Window configuration.
   */
  explicit Window(const WindowConfig &config);

  /**
   * @brief Destroy the window.
   */
  ~Window();

  // Non-copyable
  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  /**
   * @brief Check if the window was created successfully.
   */
  bool isOpen() const { return m_window != nullptr; }

  /**
   * @brief Poll and process window events.
   *
   * This should be called once per frame to update the window state.
   */
  void pollEvents();

  /**
   * @brief Check if the window should close.
   *
   * @return true if close was requested (e.g., X button, Alt+F4).
   */
  bool shouldClose() const { return m_shouldClose; }

  // --- Window Properties ---

  /**
   * @brief Get the native SDL window handle.
   */
  SDL_Window *getNativeHandle() const { return m_window; }

  /**
   * @brief Get the window size in screen coordinates (logical pixels).
   */
  void getSize(int &w, int &h) const;

  /**
   * @brief Get the drawable size in actual pixels (DPI-aware).
   *
   * This may be larger than getSize() on high-DPI displays.
   *
   * @ref specs/Chapter 5 §5.2 Note
   *      "On high-DPI displays (e.g., macOS Retina), ensure you query drawable
   *       pixel size, not window logical size."
   */
  void getDrawableSize(int &w, int &h) const;

  /**
   * @brief Get the display index this window is on.
   */
  int getDisplayIndex() const;

  // --- Fullscreen Control ---

  /**
   * @brief Get the current fullscreen mode.
   */
  FullscreenMode getFullscreenMode() const { return m_fullscreenMode; }

  /**
   * @brief Set the fullscreen mode.
   *
   * @param mode Desired fullscreen mode.
   * @return true if mode change succeeded.
   *
   * @ref specs/Chapter 5 §5.12
   *      "Borderless fullscreen (SDL_WINDOW_FULLSCREEN_DESKTOP) rather than
   *       exclusive fullscreen, to avoid mode changes."
   */
  bool setFullscreenMode(FullscreenMode mode);

  /**
   * @brief Toggle between windowed and fullscreen desktop modes.
   *
   * @return true if toggle succeeded.
   */
  bool toggleFullscreen();

  // --- Focus State ---

  /**
   * @brief Check if the window has keyboard focus.
   */
  bool hasKeyboardFocus() const { return m_hasKeyboardFocus; }

  /**
   * @brief Check if the window has mouse focus.
   */
  bool hasMouseFocus() const { return m_hasMouseFocus; }

  /**
   * @brief Check if the window is minimized.
   */
  bool isMinimized() const { return m_isMinimized; }

  // --- Resize Handling ---

  /**
   * @brief Check if the window was resized since last frame.
   *
   * This flag is cleared after each call to pollEvents().
   */
  bool wasResized() const { return m_wasResized; }

private:
  SDL_Window *m_window = nullptr;

  // State flags
  bool m_shouldClose = false;
  bool m_hasKeyboardFocus = true;
  bool m_hasMouseFocus = true;
  bool m_isMinimized = false;
  bool m_wasResized = false;

  FullscreenMode m_fullscreenMode = FullscreenMode::Windowed;

  // Saved windowed position/size for restoring from fullscreen
  int m_windowedX = 0;
  int m_windowedY = 0;
  int m_windowedW = 0;
  int m_windowedH = 0;
};

} // namespace arcanee::platform
