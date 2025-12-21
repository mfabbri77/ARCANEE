/**
 * @file Platform.cpp
 * @brief Platform layer implementation for SDL2.
 *
 * @ref specs/Chapter 2 §2.1
 *      "Platform Layer (SDL2): windowing, input, high-resolution timing,
 *       display enumeration, fullscreen control."
 *
 * @ref specs/Appendix D §D.3.1
 *      "SDL initialization/shutdown, Window creation, Drawable pixel size
 *       queries (DPI-aware), Event pump, High-resolution monotonic timing."
 */

#include "Platform.h"
#include "common/Log.h"
#include <SDL.h>

namespace arcanee::platform {

bool Platform::s_initialized = false;

bool Platform::init(const PlatformConfig &config) {
  if (s_initialized) {
    LOG_WARN("Platform::init: Already initialized");
    return true;
  }

  // Build initialization flags based on config
  Uint32 flags = SDL_INIT_TIMER | SDL_INIT_EVENTS;

  if (config.enableVideo) {
    flags |= SDL_INIT_VIDEO;
  }

  if (config.enableAudio) {
    flags |= SDL_INIT_AUDIO;
  }

  if (config.enableGamepad) {
    flags |= SDL_INIT_GAMECONTROLLER;
  }

  if (SDL_Init(flags) != 0) {
    LOG_ERROR("Platform::init: SDL_Init failed: %s", SDL_GetError());
    return false;
  }

  s_initialized = true;

  // Log platform info
  LOG_INFO("Platform: Initialized SDL2 %s on %s", getSDLVersion().c_str(),
           getPlatformName());
  LOG_INFO("Platform: %d display(s) detected", getDisplayCount());

  // Log display details in debug mode
  for (int i = 0; i < getDisplayCount(); ++i) {
    DisplayInfo info = getDisplayInfo(i);
    LOG_DEBUG("Platform: Display %d: %s (%dx%d @ %dHz, DPI scale: %.2f)%s", i,
              info.name.c_str(), info.width, info.height, info.refreshRate,
              info.dpiScale, info.isPrimary ? " [Primary]" : "");
  }

  return true;
}

void Platform::shutdown() {
  if (!s_initialized) {
    return;
  }

  SDL_Quit();
  s_initialized = false;
  LOG_INFO("Platform: Shutdown complete");
}

bool Platform::isInitialized() { return s_initialized; }

int Platform::getDisplayCount() {
  if (!s_initialized) {
    return 0;
  }
  int count = SDL_GetNumVideoDisplays();
  return (count > 0) ? count : 0;
}

DisplayInfo Platform::getDisplayInfo(int index) {
  DisplayInfo info{};
  info.index = index;

  if (!s_initialized || index < 0 || index >= getDisplayCount()) {
    return info;
  }

  // Get display name
  const char *name = SDL_GetDisplayName(index);
  info.name = name ? name : "Unknown";

  // Get desktop display mode (native resolution)
  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(index, &mode) == 0) {
    info.width = mode.w;
    info.height = mode.h;
    info.refreshRate = mode.refresh_rate;
  }

  // Get DPI scale
  // Note: SDL_GetDisplayDPI returns diagonal, horizontal, vertical DPI
  float ddpi = 0.0f, hdpi = 0.0f, vdpi = 0.0f;
  if (SDL_GetDisplayDPI(index, &ddpi, &hdpi, &vdpi) == 0) {
    // Use horizontal DPI as reference (96 DPI is "normal" on most platforms)
    info.dpiScale = (hdpi > 0.0f) ? (hdpi / 96.0f) : 1.0f;
  } else {
    info.dpiScale = 1.0f;
  }

  // Primary display is typically index 0
  info.isPrimary = (index == 0);

  return info;
}

std::vector<DisplayInfo> Platform::getAllDisplays() {
  std::vector<DisplayInfo> displays;
  int count = getDisplayCount();
  displays.reserve(count);

  for (int i = 0; i < count; ++i) {
    displays.push_back(getDisplayInfo(i));
  }

  return displays;
}

int Platform::getDisplayForWindow(SDL_Window *window) {
  if (!s_initialized || !window) {
    return -1;
  }
  return SDL_GetWindowDisplayIndex(window);
}

std::string Platform::getSDLVersion() {
  SDL_version compiled;
  SDL_version linked;

  SDL_VERSION(&compiled);
  SDL_GetVersion(&linked);

  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%d.%d.%d (compiled: %d.%d.%d)",
           linked.major, linked.minor, linked.patch, compiled.major,
           compiled.minor, compiled.patch);

  return buffer;
}

const char *Platform::getPlatformName() { return SDL_GetPlatform(); }

} // namespace arcanee::platform
