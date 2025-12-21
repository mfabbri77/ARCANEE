#pragma once

#include "common/Types.h"

namespace arcanee::render {

/**
 * @brief CBUF preset resolutions as specified in Chapter 5 §5.3.1.
 */
enum class CBufPreset {
  // 16:9 Aspect
  Ultra_16_9,  // 3840×2160
  High_16_9,   // 1920×1080
  Medium_16_9, // 960×540
  Low_16_9,    // 480×270

  // 4:3 Aspect
  Ultra_4_3,  // 3200×2400
  High_4_3,   // 1600×1200
  Medium_4_3, // 800×600
  Low_4_3     // 400×300
};

/**
 * @brief CBUF dimensions.
 */
struct CBufDimensions {
  u32 width;
  u32 height;
};

/**
 * @brief Get dimensions for a CBUF preset.
 */
inline CBufDimensions getCBufDimensions(CBufPreset preset) {
  switch (preset) {
  // 16:9
  case CBufPreset::Ultra_16_9:
    return {3840, 2160};
  case CBufPreset::High_16_9:
    return {1920, 1080};
  case CBufPreset::Medium_16_9:
    return {960, 540};
  case CBufPreset::Low_16_9:
    return {480, 270};
  // 4:3
  case CBufPreset::Ultra_4_3:
    return {3200, 2400};
  case CBufPreset::High_4_3:
    return {1600, 1200};
  case CBufPreset::Medium_4_3:
    return {800, 600};
  case CBufPreset::Low_4_3:
    return {400, 300};
  }
  return {960, 540}; // Default fallback
}

/**
 * @brief Get aspect ratio string for a preset.
 */
inline const char *getCBufAspectString(CBufPreset preset) {
  switch (preset) {
  case CBufPreset::Ultra_16_9:
  case CBufPreset::High_16_9:
  case CBufPreset::Medium_16_9:
  case CBufPreset::Low_16_9:
    return "16:9";
  case CBufPreset::Ultra_4_3:
  case CBufPreset::High_4_3:
  case CBufPreset::Medium_4_3:
  case CBufPreset::Low_4_3:
    return "4:3";
  }
  return "unknown";
}

} // namespace arcanee::render
