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
 * @file Platform.h
 * @brief Platform layer initialization and management.
 *
 * Handles SDL2 subsystem initialization, display enumeration, and platform
 * queries. This is the entry point for all platform-specific functionality.
 *
 * @ref specs/Chapter 2 — System Architecture Overview §2.1
 *      "Platform Layer (SDL2): windowing, input, high-resolution timing,
 *       display enumeration, fullscreen control."
 *
 * @ref specs/Appendix D §D.3.1
 *      "Must not call into Squirrel from other threads."
 */

#include <string>
#include <vector>

struct SDL_Window;

namespace arcanee::platform {

/**
 * @brief Display information for multi-monitor support.
 *
 * @ref specs/Chapter 5 §5.12.1
 *      "In Workbench mode, fullscreen desktop MUST apply to the currently
 *       selected display. The runtime SHOULD allow selecting the target
 * display."
 */
struct DisplayInfo {
  int index;        ///< SDL display index
  std::string name; ///< Display name (if available)
  int width;        ///< Native width in pixels
  int height;       ///< Native height in pixels
  int refreshRate;  ///< Refresh rate in Hz (0 if unknown)
  float dpiScale;   ///< DPI scale factor (1.0 = 96 DPI)
  bool isPrimary;   ///< True if this is the primary display
};

/**
 * @brief Platform initialization flags.
 */
struct PlatformConfig {
  bool enableVideo = true;   ///< Initialize video subsystem
  bool enableAudio = true;   ///< Initialize audio subsystem
  bool enableGamepad = true; ///< Initialize game controller subsystem
};

/**
 * @brief Platform layer singleton for SDL2 management.
 *
 * This class manages SDL2 initialization, display enumeration, and provides
 * platform-level queries. It must be initialized before any other platform
 * components (Window, Input, Audio).
 */
class Platform {
public:
  /**
   * @brief Initialize the platform layer with SDL2.
   *
   * @param config Configuration flags for subsystem initialization.
   * @return true if initialization succeeded, false otherwise.
   *
   * @note This should be called once at application startup.
   */
  static bool init(const PlatformConfig &config = {});

  /**
   * @brief Shutdown the platform layer and SDL2.
   *
   * @note This should be called once at application shutdown.
   */
  static void shutdown();

  /**
   * @brief Check if the platform is initialized.
   */
  static bool isInitialized();

  /**
   * @brief Get the number of attached displays.
   */
  static int getDisplayCount();

  /**
   * @brief Get information about a specific display.
   *
   * @param index Display index (0 to getDisplayCount() - 1).
   * @return DisplayInfo structure, or empty info if index is invalid.
   */
  static DisplayInfo getDisplayInfo(int index);

  /**
   * @brief Get all display information.
   */
  static std::vector<DisplayInfo> getAllDisplays();

  /**
   * @brief Get the display containing a window.
   *
   * @param window SDL window handle.
   * @return Display index, or -1 if not found.
   */
  static int getDisplayForWindow(SDL_Window *window);

  /**
   * @brief Get SDL2 version string.
   */
  static std::string getSDLVersion();

  /**
   * @brief Get the current platform name (e.g., "Windows", "Linux", "macOS").
   */
  static const char *getPlatformName();

private:
  Platform() = delete;
  static bool s_initialized;
};

} // namespace arcanee::platform
