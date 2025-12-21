#pragma once

/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

#include <algorithm>
#include <cmath>

namespace arcanee::render {

// Present mode types per Chapter 5 §5.8
enum class PresentMode { Fit, IntegerNearest, Fill, Stretch };

// Viewport calculation result
struct Viewport {
  int x, y, w, h;
};

// Calculate viewport based on present mode
// Reference implementation from Chapter 5 §5.8.7 (Normative)
inline Viewport calculateViewport(int W, int H, // Backbuffer
                                  int w, int h, // CBUF
                                  PresentMode mode, int *outScale = nullptr) {
  Viewport vp{0, 0, W, H};

  switch (mode) {
  case PresentMode::Fit: {
    double s = std::min(static_cast<double>(W) / w, static_cast<double>(H) / h);
    vp.w = static_cast<int>(std::floor(w * s));
    vp.h = static_cast<int>(std::floor(h * s));
    vp.x = static_cast<int>(std::floor((W - vp.w) / 2.0));
    vp.y = static_cast<int>(std::floor((H - vp.h) / 2.0));
    break;
  }

  case PresentMode::IntegerNearest: {
    int k = std::min(W / w, H / h);
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
    double s = std::max(static_cast<double>(W) / w, static_cast<double>(H) / h);
    vp.w = static_cast<int>(std::ceil(w * s));
    vp.h = static_cast<int>(std::ceil(h * s));
    vp.x = static_cast<int>(std::floor((W - vp.w) / 2.0));
    vp.y = static_cast<int>(std::floor((H - vp.h) / 2.0));
    break;
  }

  case PresentMode::Stretch:
    vp = {0, 0, W, H};
    break;
  }

  return vp;
}

// Letterbox/pillarbox clear color (Chapter 5 §5.8.6)
constexpr unsigned int kLetterboxColor = 0xFF000000; // Opaque black (ARGB)

} // namespace arcanee::render
