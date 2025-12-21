#include "Framebuffer.h"
#include "RenderDevice.h"
#include "common/Log.h"

// Diligent includes (isolated in .cpp)
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"

namespace arcanee::render {

using namespace Diligent;

struct Framebuffer::Impl {
  RefCntAutoPtr<ITexture> pColorTexture;
  RefCntAutoPtr<ITexture> pDepthTexture;
  ITextureView *pRTV = nullptr; // Owned by texture
  ITextureView *pSRV = nullptr; // Owned by texture
  ITextureView *pDSV = nullptr; // Owned by texture
};

Framebuffer::Framebuffer() : m_impl(new Impl()) {}

Framebuffer::~Framebuffer() {
  delete m_impl;
  m_impl = nullptr;
}

bool Framebuffer::create(RenderDevice &device, u32 width, u32 height,
                         bool withDepth) {
  auto *pDevice = static_cast<IRenderDevice *>(device.getDevice());
  if (!pDevice) {
    LOG_ERROR("Framebuffer::create: Invalid device");
    return false;
  }

  m_width = width;
  m_height = height;
  m_hasDepth = withDepth;

  // Create color texture
  TextureDesc colorTexDesc;
  colorTexDesc.Name = "CBUF Color";
  colorTexDesc.Type = RESOURCE_DIM_TEX_2D;
  colorTexDesc.Width = width;
  colorTexDesc.Height = height;
  colorTexDesc.Format =
      TEX_FORMAT_RGBA8_UNORM; // Per §5.5.2 (BGRA preferred but RGBA fallback)
  colorTexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
  colorTexDesc.MipLevels = 1;
  colorTexDesc.SampleCount = 1;

  pDevice->CreateTexture(colorTexDesc, nullptr, &m_impl->pColorTexture);
  if (!m_impl->pColorTexture) {
    LOG_ERROR("Framebuffer::create: Failed to create color texture");
    return false;
  }

  m_impl->pRTV =
      m_impl->pColorTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  m_impl->pSRV =
      m_impl->pColorTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

  // Create depth texture if requested
  if (withDepth) {
    TextureDesc depthTexDesc;
    depthTexDesc.Name = "CBUF Depth";
    depthTexDesc.Type = RESOURCE_DIM_TEX_2D;
    depthTexDesc.Width = width;
    depthTexDesc.Height = height;
    depthTexDesc.Format = TEX_FORMAT_D24_UNORM_S8_UINT;
    depthTexDesc.BindFlags = BIND_DEPTH_STENCIL;
    depthTexDesc.MipLevels = 1;
    depthTexDesc.SampleCount = 1;

    pDevice->CreateTexture(depthTexDesc, nullptr, &m_impl->pDepthTexture);
    if (!m_impl->pDepthTexture) {
      LOG_WARN("Framebuffer::create: Failed to create depth texture");
      m_hasDepth = false;
    } else {
      m_impl->pDSV =
          m_impl->pDepthTexture->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    }
  }

  LOG_INFO("Framebuffer created: %ux%u (depth: %s)", width, height,
           m_hasDepth ? "yes" : "no");
  return true;
}

bool Framebuffer::resize(RenderDevice &device, u32 width, u32 height) {
  // Release old resources
  m_impl->pColorTexture.Release();
  m_impl->pDepthTexture.Release();
  m_impl->pRTV = nullptr;
  m_impl->pSRV = nullptr;
  m_impl->pDSV = nullptr;

  // Recreate
  return create(device, width, height, m_hasDepth);
}

void Framebuffer::clear(void *deviceContext, f32 r, f32 g, f32 b, f32 a) {
  auto *pContext = static_cast<IDeviceContext *>(deviceContext);
  if (!pContext || !m_impl->pRTV)
    return;

  // Bind render targets before clearing (Required for OpenGL / Best Practice)
  ITextureView *pRTVs[] = {m_impl->pRTV};
  pContext->SetRenderTargets(1, pRTVs, m_impl->pDSV,
                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  float clearColor[] = {r, g, b, a};
  pContext->ClearRenderTarget(m_impl->pRTV, clearColor,
                              RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  if (m_impl->pDSV) {
    pContext->ClearDepthStencil(m_impl->pDSV,
                                CLEAR_DEPTH_FLAG | CLEAR_STENCIL_FLAG, 1.0f, 0,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }
}

void *Framebuffer::getRenderTargetView() {
  return m_impl ? m_impl->pRTV : nullptr;
}

void *Framebuffer::getShaderResourceView() {
  return m_impl ? m_impl->pSRV : nullptr;
}

void *Framebuffer::getDepthStencilView() {
  return m_impl ? m_impl->pDSV : nullptr;
}

bool Framebuffer::isValid() const {
  return m_impl && m_impl->pColorTexture && m_impl->pRTV && m_impl->pSRV;
}

} // namespace arcanee::render
