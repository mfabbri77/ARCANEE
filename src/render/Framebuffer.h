#pragma once

#include "common/Types.h"

namespace arcanee::render {

// Forward declare Diligent types
class RenderDevice;

/**
 * @brief GPU framebuffer wrapper for CBUF and offscreen surfaces.
 *
 * Encapsulates a Diligent render target texture with optional depth buffer.
 * Uses PIMPL to isolate Diligent headers.
 */
class Framebuffer {
public:
  Framebuffer();
  ~Framebuffer();

  // Non-copyable
  Framebuffer(const Framebuffer &) = delete;
  Framebuffer &operator=(const Framebuffer &) = delete;

  /**
   * @brief Create the framebuffer with specified dimensions.
   * @param device RenderDevice to use for creation
   * @param width Width in pixels
   * @param height Height in pixels
   * @param withDepth Whether to create a depth buffer
   * @return true if successful
   */
  bool create(RenderDevice &device, u32 width, u32 height,
              bool withDepth = false);

  /**
   * @brief Resize the framebuffer (recreates textures).
   */
  bool resize(RenderDevice &device, u32 width, u32 height);

  /**
   * @brief Clear the framebuffer to a color.
   */
  void clear(void *deviceContext, f32 r, f32 g, f32 b, f32 a);

  /**
   * @brief Get the render target view for binding as render target.
   */
  void *getRenderTargetView();

  /**
   * @brief Get the shader resource view for sampling in shaders.
   */
  void *getShaderResourceView();

  /**
   * @brief Get the depth stencil view (if created with depth).
   */
  void *getDepthStencilView();

  u32 getWidth() const { return m_width; }
  u32 getHeight() const { return m_height; }
  bool hasDepth() const { return m_hasDepth; }
  bool isValid() const;

private:
  struct Impl;
  Impl *m_impl = nullptr;

  u32 m_width = 0;
  u32 m_height = 0;
  bool m_hasDepth = false;
};

} // namespace arcanee::render
