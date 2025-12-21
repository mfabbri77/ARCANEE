#include "Cartridge.h"

namespace arcanee::runtime {

const char *cartridgeStateToString(CartridgeState state) {
  switch (state) {
  case CartridgeState::Unloaded:
    return "Unloaded";
  case CartridgeState::Loading:
    return "Loading";
  case CartridgeState::Initialized:
    return "Initialized";
  case CartridgeState::Running:
    return "Running";
  case CartridgeState::Paused:
    return "Paused";
  case CartridgeState::Faulted:
    return "Faulted";
  case CartridgeState::Stopped:
    return "Stopped";
  }
  return "Unknown";
}

// CBUF preset dimensions from Chapter 5 §5.3.1
void getCbufDimensions(CartridgeConfig::Display::Aspect aspect,
                       CartridgeConfig::Display::Preset preset, int &outWidth,
                       int &outHeight) {
  using Aspect = CartridgeConfig::Display::Aspect;
  using Preset = CartridgeConfig::Display::Preset;

  if (aspect == Aspect::Ratio16x9 || aspect == Aspect::Any) {
    switch (preset) {
    case Preset::Low:
      outWidth = 480;
      outHeight = 270;
      break;
    case Preset::Medium:
      outWidth = 960;
      outHeight = 540;
      break;
    case Preset::High:
      outWidth = 1920;
      outHeight = 1080;
      break;
    case Preset::Ultra:
      outWidth = 3840;
      outHeight = 2160;
      break;
    }
  } else { // 4:3
    switch (preset) {
    case Preset::Low:
      outWidth = 400;
      outHeight = 300;
      break;
    case Preset::Medium:
      outWidth = 800;
      outHeight = 600;
      break;
    case Preset::High:
      outWidth = 1600;
      outHeight = 1200;
      break;
    case Preset::Ultra:
      outWidth = 3200;
      outHeight = 2400;
      break;
    }
  }
}

} // namespace arcanee::runtime
