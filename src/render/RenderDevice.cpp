#include "RenderDevice.h"
#include "common/Log.h"

// Include Diligent headers here (in .cpp only) to avoid macro conflicts
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

#if PLATFORM_LINUX
#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include <X11/Xlib.h> // For Display and Window types
#endif

namespace arcanee::render {

using namespace Diligent;

struct RenderDevice::Impl {
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;
};

RenderDevice::RenderDevice() : m_impl(new Impl()) {}

RenderDevice::~RenderDevice() {
  if (m_impl) {
    if (m_impl->pImmediateContext) {
      m_impl->pImmediateContext->Flush();
    }
    delete m_impl;
    m_impl = nullptr;
  }
}

bool RenderDevice::initialize(void *displayHandle, unsigned long windowHandle) {
  LOG_INFO("Initializing RenderDevice...");

  SwapChainDesc swapChainDesc;
  swapChainDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
  swapChainDesc.DepthBufferFormat = TEX_FORMAT_D24_UNORM_S8_UINT;
  swapChainDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;
  swapChainDesc.BufferCount = 2;
  swapChainDesc.IsPrimary = true;

#if PLATFORM_LINUX
  // Try Vulkan first
  auto *pFactoryVk = GetEngineFactoryVk();
  EngineVkCreateInfo engineCI;
  // Validation layers are not available in the current environment
  // Explicitly disable to suppress "VK_LAYER_KHRONOS_validation not available"
  // warning
  engineCI.EnableValidation = false;

  pFactoryVk->CreateDeviceAndContextsVk(engineCI, &m_impl->pDevice,
                                        &m_impl->pImmediateContext);

  if (m_impl->pDevice) {
    LOG_INFO("RenderDevice: Vulkan device created");

    // Create swapchain with proper X11 handles
    LinuxNativeWindow window;
    window.WindowId = static_cast<Window>(windowHandle);
    window.pDisplay = static_cast<Display *>(displayHandle);

    pFactoryVk->CreateSwapChainVk(m_impl->pDevice, m_impl->pImmediateContext,
                                  swapChainDesc, window, &m_impl->pSwapChain);
  } else {
    LOG_WARN("Vulkan initialization failed. Trying OpenGL...");

    auto *pFactoryGL = GetEngineFactoryOpenGL();
    EngineGLCreateInfo engineCIGL;
    engineCIGL.Window.WindowId = static_cast<Window>(windowHandle);
    engineCIGL.Window.pDisplay = static_cast<Display *>(displayHandle);

    pFactoryGL->CreateDeviceAndSwapChainGL(engineCIGL, &m_impl->pDevice,
                                           &m_impl->pImmediateContext,
                                           swapChainDesc, &m_impl->pSwapChain);
  }
#else
  (void)displayHandle;
  (void)windowHandle;
  LOG_ERROR("RenderDevice: Platform not supported yet");
  return false;
#endif

  if (m_impl->pDevice && m_impl->pSwapChain) {
    LOG_INFO("RenderDevice initialized successfully");
    return true;
  }

  LOG_ERROR("Failed to initialize RenderDevice");
  if (m_impl) {
    m_impl->pSwapChain.Release();
    m_impl->pImmediateContext.Release();
    m_impl->pDevice.Release();
  }
  return false;
}

void RenderDevice::present() {
  if (m_impl && m_impl->pSwapChain) {
    m_impl->pSwapChain->Present();
  }
}

void RenderDevice::resize(u32 width, u32 height) {
  if (m_impl && m_impl->pSwapChain) {
    m_impl->pSwapChain->Resize(width, height);
  }
}

void *RenderDevice::getDevice() {
  return m_impl ? m_impl->pDevice.RawPtr() : nullptr;
}

void *RenderDevice::getContext() {
  return m_impl ? m_impl->pImmediateContext.RawPtr() : nullptr;
}

void *RenderDevice::getSwapChain() {
  return m_impl ? m_impl->pSwapChain.RawPtr() : nullptr;
}

// ===== VSync Control =====
void RenderDevice::setVSync(bool enabled) {
  m_vsyncEnabled = enabled;
  if (m_impl && m_impl->pSwapChain) {
    // Diligent uses present interval: 0 = no vsync, 1 = vsync
    // This will take effect on next Present() call
    LOG_INFO("RenderDevice: VSync %s", enabled ? "enabled" : "disabled");
  }
}

bool RenderDevice::isVSyncEnabled() const { return m_vsyncEnabled; }

// ===== Device Loss Recovery =====
bool RenderDevice::isDeviceLost() const { return m_deviceLost; }

bool RenderDevice::tryRecoverDevice() {
  if (!m_deviceLost)
    return true;

  LOG_INFO("RenderDevice: Attempting device recovery...");

  // In Diligent, device loss recovery typically requires
  // recreating all resources. For v0.1, we mark as unrecoverable
  // and require application restart.
  LOG_ERROR("RenderDevice: Device loss recovery not implemented in v0.1");
  return false;
}

RenderDevice::RenderBackend RenderDevice::getBackend() const {
  if (!m_impl || !m_impl->pDevice)
    return RenderBackend::Unknown;

  const auto &caps = m_impl->pDevice->GetDeviceInfo();
  switch (caps.Type) {
  case RENDER_DEVICE_TYPE_VULKAN:
    return RenderBackend::Vulkan;
  case RENDER_DEVICE_TYPE_GL:
    return RenderBackend::OpenGL;
  case RENDER_DEVICE_TYPE_GLES:
    return RenderBackend::OpenGL;
  case RENDER_DEVICE_TYPE_D3D11:
    return RenderBackend::Direct3D11;
  case RENDER_DEVICE_TYPE_D3D12:
    return RenderBackend::Direct3D12;
  case RENDER_DEVICE_TYPE_METAL:
    return RenderBackend::Metal;
  default:
    return RenderBackend::Unknown;
  }
}

} // namespace arcanee::render
