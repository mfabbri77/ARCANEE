#pragma once

#include "common/Types.h"
#include <array>
#include <cmath>
#include <vector>

namespace arcanee::render {

/**
 * @brief 2D affine transform matrix per spec §6.5.1
 *
 * | a c e |
 * | b d f |
 * | 0 0 1 |
 */
struct Transform2D {
  f32 a = 1.0f, b = 0.0f; // first column
  f32 c = 0.0f, d = 1.0f; // second column
  f32 e = 0.0f, f = 0.0f; // translation

  static Transform2D identity() { return {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}; }

  Transform2D operator*(const Transform2D &rhs) const {
    Transform2D result;
    result.a = a * rhs.a + c * rhs.b;
    result.b = b * rhs.a + d * rhs.b;
    result.c = a * rhs.c + c * rhs.d;
    result.d = b * rhs.c + d * rhs.d;
    result.e = a * rhs.e + c * rhs.f + e;
    result.f = b * rhs.e + d * rhs.f + f;
    return result;
  }

  void translate(f32 tx, f32 ty) {
    e += a * tx + c * ty;
    f += b * tx + d * ty;
  }

  void scale(f32 sx, f32 sy) {
    a *= sx;
    c *= sy;
    b *= sx;
    d *= sy;
  }

  void rotate(f32 rad) {
    f32 cos_r = cosf(rad);
    f32 sin_r = sinf(rad);
    f32 na = a * cos_r + c * sin_r;
    f32 nb = b * cos_r + d * sin_r;
    f32 nc = a * -sin_r + c * cos_r;
    f32 nd = b * -sin_r + d * cos_r;
    a = na;
    b = nb;
    c = nc;
    d = nd;
  }
};

/**
 * @brief Line join style
 */
enum class LineJoin : u8 { Miter, Round, Bevel };

/**
 * @brief Line cap style
 */
enum class LineCap : u8 { Butt, Round, Square };

/**
 * @brief Blend mode
 */
enum class BlendMode : u8 {
  Normal,
  Multiply,
  Screen,
  Overlay,
  Darken,
  Lighten,
  // ... more can be added
};

/**
 * @brief Text alignment
 */
enum class TextAlign : u8 { Left, Center, Right, Start, End };

/**
 * @brief Text baseline
 */
enum class TextBaseline : u8 { Top, Middle, Alphabetic, Bottom };

/**
 * @brief Canvas drawing state per spec §6.4.1
 *
 * Saved/restored via gfx.save() and gfx.restore()
 */
struct CanvasState {
  // Transform
  Transform2D transform = Transform2D::identity();

  // Global compositing
  f32 globalAlpha = 1.0f;
  BlendMode blendMode = BlendMode::Normal;

  // Fill/stroke style
  u32 fillColor = 0xFFFFFFFF;   // opaque white
  u32 strokeColor = 0xFF000000; // opaque black

  // Stroke parameters
  f32 lineWidth = 1.0f;
  LineJoin lineJoin = LineJoin::Miter;
  LineCap lineCap = LineCap::Butt;
  f32 miterLimit = 10.0f;

  // Dash pattern (empty = solid)
  std::vector<f32> lineDash;
  f32 lineDashOffset = 0.0f;

  // Text (handle = 0 means no font)
  u32 fontHandle = 0;
  TextAlign textAlign = TextAlign::Left;
  TextBaseline textBaseline = TextBaseline::Alphabetic;

  // Clip (simplified: just track if clipping is active)
  bool hasClip = false;
};

/**
 * @brief Manages canvas state stack
 */
class CanvasStateStack {
public:
  CanvasStateStack() {
    m_stack.push_back(CanvasState{}); // default state
  }

  CanvasState &current() { return m_stack.back(); }

  const CanvasState &current() const { return m_stack.back(); }

  void save() {
    m_stack.push_back(m_stack.back()); // duplicate top
  }

  bool restore() {
    if (m_stack.size() <= 1) {
      return false; // can't pop below default
    }
    m_stack.pop_back();
    return true;
  }

  void reset() {
    m_stack.clear();
    m_stack.push_back(CanvasState{});
  }

private:
  std::vector<CanvasState> m_stack;
};

} // namespace arcanee::render
