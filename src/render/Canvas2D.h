#pragma once

#include "CanvasState.h"
#include "common/Types.h"

namespace arcanee::render {

class RenderDevice;

/**
 * @brief 2D Canvas using ThorVG for vector graphics.
 *
 * Performs CPU rasterization to a 32bpp surface, then uploads
 * to a GPU texture for compositing over CBUF.
 *
 * @ref specs/Chapter 6 §6.1
 */
class Canvas2D {
public:
  Canvas2D();
  ~Canvas2D();

  // Non-copyable
  Canvas2D(const Canvas2D &) = delete;
  Canvas2D &operator=(const Canvas2D &) = delete;

  // ===== Lifecycle =====
  bool initialize(RenderDevice &device, u32 width, u32 height);
  bool resize(RenderDevice &device, u32 width, u32 height);
  void beginFrame();
  void endFrame(RenderDevice &device);

  // ===== Target & Clearing (§6.3.1) =====
  void clear(u32 color = 0x00000000);
  u32 getWidth() const { return m_width; }
  u32 getHeight() const { return m_height; }

  // ===== State Stack (§6.3.2) =====
  void save();
  void restore();

  // ===== Transforms (§6.5) =====
  void resetTransform();
  void setTransform(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f);
  void translate(f32 x, f32 y);
  void rotate(f32 radians);
  void scale(f32 sx, f32 sy);

  // ===== Global State (§6.3.2) =====
  void setGlobalAlpha(f32 alpha);
  void setBlendMode(BlendMode mode);

  // ===== Styles (§6.3.3) =====
  void setFillColor(u32 color);
  void setStrokeColor(u32 color);
  void setLineWidth(f32 width);
  void setLineJoin(LineJoin join);
  void setLineCap(LineCap cap);
  void setMiterLimit(f32 limit);

  // ===== Paths (§6.3.4) =====
  void beginPath();
  void closePath();
  void moveTo(f32 x, f32 y);
  void lineTo(f32 x, f32 y);
  void quadTo(f32 cx, f32 cy, f32 x, f32 y);
  void cubicTo(f32 c1x, f32 c1y, f32 c2x, f32 c2y, f32 x, f32 y);
  void arc(f32 x, f32 y, f32 r, f32 startAngle, f32 endAngle, bool ccw = false);
  void rect(f32 x, f32 y, f32 w, f32 h);

  // ===== Drawing (§6.3.5) =====
  void fill();
  void stroke();
  void fillRect(f32 x, f32 y, f32 w, f32 h);
  void strokeRect(f32 x, f32 y, f32 w, f32 h);
  void clearRect(f32 x, f32 y, f32 w, f32 h);

  // ===== GPU Interface =====
  void *getShaderResourceView();
  bool isValid() const;

private:
  struct Impl;
  Impl *m_impl = nullptr;

  u32 m_width = 0;
  u32 m_height = 0;
  CanvasStateStack m_stateStack;
};

} // namespace arcanee::render
