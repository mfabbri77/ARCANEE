#include "Canvas2D.h"
#include "RenderDevice.h"
#include "common/Log.h"

#include <cstring>
#include <thorvg.h>

// Diligent includes (isolated in .cpp)
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"

namespace arcanee::render {

using namespace Diligent;

struct Canvas2D::Impl {
  // ThorVG
  std::unique_ptr<tvg::SwCanvas> canvas;

  // CPU buffer (32bpp ARGB)
  std::vector<u32> cpuBuffer;

  // GPU texture for upload
  RefCntAutoPtr<ITexture> pTexture;
  ITextureView *pSRV = nullptr;

  // Current shapes for drawing
  tvg::Shape *currentRect = nullptr;
};

Canvas2D::Canvas2D() : m_impl(new Impl()) {}

Canvas2D::~Canvas2D() {
  if (m_impl) {
    m_impl->canvas.reset();
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
    delete m_impl;
    m_impl = nullptr;
  }
}

bool Canvas2D::initialize(RenderDevice &device, u32 width, u32 height) {
  // Initialize ThorVG
  if (tvg::Initializer::init(tvg::CanvasEngine::Sw, 0) !=
      tvg::Result::Success) {
    LOG_ERROR("Canvas2D: Failed to initialize ThorVG");
    return false;
  }

  m_width = width;
  m_height = height;

  // Create CPU buffer
  m_impl->cpuBuffer.resize(width * height);
  std::memset(m_impl->cpuBuffer.data(), 0, width * height * sizeof(u32));

  // Create ThorVG software canvas
  m_impl->canvas = tvg::SwCanvas::gen();
  if (!m_impl->canvas) {
    LOG_ERROR("Canvas2D: Failed to create ThorVG canvas");
    return false;
  }

  // Set canvas target to our CPU buffer
  m_impl->canvas->target(m_impl->cpuBuffer.data(), width, width, height,
                         tvg::SwCanvas::ARGB8888);

  // Create GPU texture for upload
  auto *pDevice = static_cast<IRenderDevice *>(device.getDevice());
  if (!pDevice) {
    LOG_ERROR("Canvas2D: Invalid device");
    return false;
  }

  TextureDesc texDesc;
  texDesc.Name = "Canvas2D Texture";
  texDesc.Type = RESOURCE_DIM_TEX_2D;
  texDesc.Width = width;
  texDesc.Height = height;
  texDesc.Format =
      TEX_FORMAT_BGRA8_UNORM; // ThorVG outputs ARGB, but we'll handle swizzle
  texDesc.BindFlags = BIND_SHADER_RESOURCE;
  texDesc.Usage = USAGE_DEFAULT;
  texDesc.MipLevels = 1;
  texDesc.SampleCount = 1;

  pDevice->CreateTexture(texDesc, nullptr, &m_impl->pTexture);
  if (!m_impl->pTexture) {
    LOG_ERROR("Canvas2D: Failed to create GPU texture");
    return false;
  }

  m_impl->pSRV = m_impl->pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

  LOG_INFO("Canvas2D: ThorVG initialized (%ux%u)", width, height);
  return true;
}

bool Canvas2D::resize(RenderDevice &device, u32 width, u32 height) {
  // Release old resources
  m_impl->canvas.reset();
  m_impl->pTexture.Release();
  m_impl->pSRV = nullptr;
  m_impl->cpuBuffer.clear();

  // Reinitialize (ThorVG already initialized)
  m_width = width;
  m_height = height;

  m_impl->cpuBuffer.resize(width * height);
  std::memset(m_impl->cpuBuffer.data(), 0, width * height * sizeof(u32));

  m_impl->canvas = tvg::SwCanvas::gen();
  if (!m_impl->canvas)
    return false;

  m_impl->canvas->target(m_impl->cpuBuffer.data(), width, width, height,
                         tvg::SwCanvas::ARGB8888);

  auto *pDevice = static_cast<IRenderDevice *>(device.getDevice());

  TextureDesc texDesc;
  texDesc.Name = "Canvas2D Texture";
  texDesc.Type = RESOURCE_DIM_TEX_2D;
  texDesc.Width = width;
  texDesc.Height = height;
  texDesc.Format = TEX_FORMAT_BGRA8_UNORM;
  texDesc.BindFlags = BIND_SHADER_RESOURCE;
  texDesc.Usage = USAGE_DEFAULT;
  texDesc.MipLevels = 1;
  texDesc.SampleCount = 1;

  pDevice->CreateTexture(texDesc, nullptr, &m_impl->pTexture);
  if (!m_impl->pTexture)
    return false;

  m_impl->pSRV = m_impl->pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

  LOG_INFO("Canvas2D: Resized to %ux%u", width, height);
  return true;
}

void Canvas2D::beginFrame() {
  // Clear canvas for new frame
  if (m_impl && m_impl->canvas) {
    m_impl->canvas->clear(true);
  }
}

void Canvas2D::endFrame(RenderDevice &device) {
  if (!m_impl || !m_impl->canvas)
    return;

  // Render all queued shapes
  m_impl->canvas->draw();
  m_impl->canvas->sync();

  // Upload CPU buffer to GPU texture
  auto *pContext = static_cast<IDeviceContext *>(device.getContext());
  if (!pContext || !m_impl->pTexture)
    return;

  Box updateBox;
  updateBox.MinX = 0;
  updateBox.MinY = 0;
  updateBox.MinZ = 0;
  updateBox.MaxX = m_width;
  updateBox.MaxY = m_height;
  updateBox.MaxZ = 1;

  TextureSubResData subResData;
  subResData.pData = m_impl->cpuBuffer.data();
  subResData.Stride = m_width * sizeof(u32);

  pContext->UpdateTexture(m_impl->pTexture, 0, 0, updateBox, subResData,
                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void Canvas2D::clear(u32 color) {
  if (!m_impl || !m_impl->canvas)
    return;

  // Fill canvas with solid color using a rect
  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, static_cast<float>(m_width),
                 static_cast<float>(m_height));

  // Extract ARGB components
  u8 a = (color >> 24) & 0xFF;
  u8 r = (color >> 16) & 0xFF;
  u8 g = (color >> 8) & 0xFF;
  u8 b = color & 0xFF;

  bg->fill(r, g, b, a);
  m_impl->canvas->push(std::move(bg));
}

void Canvas2D::fillRect(f32 x, f32 y, f32 w, f32 h) {
  if (!m_impl || !m_impl->canvas)
    return;

  auto rect = tvg::Shape::gen();
  rect->appendRect(x, y, w, h);

  // Extract fill color ARGB
  u8 a = (m_fillColor >> 24) & 0xFF;
  u8 r = (m_fillColor >> 16) & 0xFF;
  u8 g = (m_fillColor >> 8) & 0xFF;
  u8 b = m_fillColor & 0xFF;

  rect->fill(r, g, b, a);
  m_impl->canvas->push(std::move(rect));
}

void Canvas2D::setFillColor(u32 color) { m_fillColor = color; }

void *Canvas2D::getShaderResourceView() {
  return m_impl ? m_impl->pSRV : nullptr;
}

bool Canvas2D::isValid() const {
  return m_impl && m_impl->canvas && m_impl->pTexture && m_impl->pSRV;
}

} // namespace arcanee::render
