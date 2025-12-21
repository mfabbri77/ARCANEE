#pragma once

#include "common/Types.h"

namespace arcanee::render {

/**
 * @brief RenderDevice wrapper for DiligentEngine.
 *
 * Abstracts the Diligent GPU device, context, and swapchain.
 * PIMPL pattern used to avoid exposing Diligent headers.
 */
class RenderDevice {
public:
  RenderDevice();
  ~RenderDevice();

  // Non-copyable
  RenderDevice(const RenderDevice &) = delete;
  RenderDevice &operator=(const RenderDevice &) = delete;

  bool initialize(void *displayHandle, unsigned long windowHandle);
  void present();
  void resize(u32 width, u32 height);

  // Accessors (opaque pointers for now)
  void *getDevice();
  void *getContext();
  void *getSwapChain();

private:
  struct Impl;
  Impl *m_impl = nullptr;
};

} // namespace arcanee::render
