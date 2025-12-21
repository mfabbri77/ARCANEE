#pragma once

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

  /**
   * @brief Initialize the canvas with specified dimensions.
   */
  bool initialize(RenderDevice &device, u32 width, u32 height);

  /**
   * @brief Resize the canvas (recreates buffers).
   */
  bool resize(RenderDevice &device, u32 width, u32 height);

  /**
   * @brief Begin a new frame for drawing.
   */
  void beginFrame();

  /**
   * @brief End the frame and upload to GPU texture.
   */
  void endFrame(RenderDevice &device);

  /**
   * @brief Clear the canvas with a color.
   * @param color ARGB format (0xAARRGGBB)
   */
  void clear(u32 color = 0x00000000);

  /**
   * @brief Fill a rectangle with the current fill color.
   */
  void fillRect(f32 x, f32 y, f32 w, f32 h);

  /**
   * @brief Set the fill color.
   * @param color ARGB format (0xAARRGGBB)
   */
  void setFillColor(u32 color);

  /**
   * @brief Get the GPU texture for compositing.
   */
  void *getShaderResourceView();

  u32 getWidth() const { return m_width; }
  u32 getHeight() const { return m_height; }
  bool isValid() const;

private:
  struct Impl;
  Impl *m_impl = nullptr;

  u32 m_width = 0;
  u32 m_height = 0;
  u32 m_fillColor = 0xFFFFFFFF; // White default
};

} // namespace arcanee::render
