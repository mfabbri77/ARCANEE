#pragma once

#include "PresentMode.h"
#include "common/Types.h"

namespace arcanee::render {

class RenderDevice;

/**
 * @brief Present pass that scales CBUF to backbuffer.
 *
 * Implements the four present modes from Chapter 5 §5.8.
 * Uses PIMPL to isolate Diligent headers.
 */
class PresentPass {
public:
  PresentPass();
  ~PresentPass();

  // Non-copyable
  PresentPass(const PresentPass &) = delete;
  PresentPass &operator=(const PresentPass &) = delete;

  /**
   * @brief Initialize shader resources and pipeline.
   */
  bool initialize(RenderDevice &device);

  /**
   * @brief Execute the present pass.
   * @param device RenderDevice
   * @param cbufSRV Shader resource view of CBUF texture
   * @param cbufWidth CBUF width
   * @param cbufHeight CBUF height
   * @param mode Present mode
   */
  void execute(RenderDevice &device, void *cbufSRV, u32 cbufWidth,
               u32 cbufHeight, PresentMode mode);

  /**
   * @brief Set the letterbox clear color (default: opaque black per §5.8.6).
   */
  void setLetterboxColor(f32 r, f32 g, f32 b, f32 a);

private:
  struct Impl;
  Impl *m_impl = nullptr;

  f32 m_letterboxColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace arcanee::render
