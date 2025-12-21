#pragma once

#include "common/Types.h"
#include <algorithm>
#include <cmath>

namespace arcanee::render {

/**
 * @brief Present mode as specified in Chapter 5 §5.8.
 */
enum class PresentMode {
  Fit,            ///< Aspect-preserving letterbox (§5.8.2)
  IntegerNearest, ///< Pixel-perfect integer scaling (§5.8.3)
  Fill,           ///< Aspect-preserving crop (§5.8.4)
  Stretch         ///< Non-aspect-preserving stretch (§5.8.5)
};

/**
 * @brief Viewport rectangle in display space.
 */
struct Viewport {
  i32 x, y;
  i32 w, h;
};

/**
 * @brief Calculate present viewport per §5.8.7 reference implementation.
 *
 * @param W Backbuffer width
 * @param H Backbuffer height
 * @param w CBUF width
 * @param h CBUF height
 * @param mode Present mode
 * @param outScale Optional output for integer scale factor (integer_nearest
 * only)
 * @return Viewport rectangle
 */
inline Viewport calculateViewport(i32 W, i32 H, i32 w, i32 h, PresentMode mode,
                                  i32 *outScale = nullptr) {
  Viewport vp;

  switch (mode) {
  case PresentMode::Fit: {
    double s = std::min(double(W) / w, double(H) / h);
    vp.w = static_cast<i32>(std::floor(w * s));
    vp.h = static_cast<i32>(std::floor(h * s));
    vp.x = static_cast<i32>(std::floor((W - vp.w) / 2.0));
    vp.y = static_cast<i32>(std::floor((H - vp.h) / 2.0));
    break;
  }

  case PresentMode::IntegerNearest: {
    i32 k = std::min(W / w, H / h);
    if (k < 1) {
      // Fallback to fit when window too small
      return calculateViewport(W, H, w, h, PresentMode::Fit, nullptr);
    }
    vp.w = w * k;
    vp.h = h * k;
    vp.x = (W - vp.w) / 2;
    vp.y = (H - vp.h) / 2;
    if (outScale)
      *outScale = k;
    break;
  }

  case PresentMode::Fill: {
    double s = std::max(double(W) / w, double(H) / h);
    vp.w = static_cast<i32>(std::ceil(w * s));
    vp.h = static_cast<i32>(std::ceil(h * s));
    vp.x = static_cast<i32>(std::floor((W - vp.w) / 2.0));
    vp.y = static_cast<i32>(std::floor((H - vp.h) / 2.0));
    break;
  }

  case PresentMode::Stretch:
    vp = {0, 0, W, H};
    break;
  }

  return vp;
}

/**
 * @brief Get present mode name string.
 */
inline const char *getPresentModeName(PresentMode mode) {
  switch (mode) {
  case PresentMode::Fit:
    return "fit";
  case PresentMode::IntegerNearest:
    return "integer_nearest";
  case PresentMode::Fill:
    return "fill";
  case PresentMode::Stretch:
    return "stretch";
  }
  return "unknown";
}

} // namespace arcanee::render
