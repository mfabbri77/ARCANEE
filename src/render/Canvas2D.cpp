#include "Canvas2D.h"
#include "RenderDevice.h"
#include "common/Log.h"

#include <cmath>
#include <cstring>
#include <string>
#include <thorvg.h>
#include <unordered_map>

// Diligent includes (isolated in .cpp)
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"

namespace arcanee::render {

using namespace Diligent;

struct Canvas2D::Impl {
  // ThorVG canvas
  std::unique_ptr<tvg::SwCanvas> canvas;

  // Current path being built
  std::unique_ptr<tvg::Shape> currentPath;

  // CPU buffer (32bpp ARGB)
  std::vector<u32> cpuBuffer;

  // GPU texture for upload
  RefCntAutoPtr<ITexture> pTexture;
  ITextureView *pSRV = nullptr;

  // Image resources (handle -> Picture data)
  std::unordered_map<u32, std::unique_ptr<tvg::Picture>> images;
  u32 nextImageHandle = 1;

  // Font resources (handle -> font path + size for Text creation)
  struct FontInfo {
    std::string path;
    i32 sizePx;
  };
  std::unordered_map<u32, FontInfo> fonts;
  u32 nextFontHandle = 1;
  u32 currentFontHandle = 0;
};

// Helper to extract ARGB color components
static void colorToRGBA(u32 color, u8 &r, u8 &g, u8 &b, u8 &a) {
  a = (color >> 24) & 0xFF;
  r = (color >> 16) & 0xFF;
  g = (color >> 8) & 0xFF;
  b = color & 0xFF;
}

Canvas2D::Canvas2D() : m_impl(new Impl()) {}

Canvas2D::~Canvas2D() {
  if (m_impl) {
    m_impl->canvas.reset();
    m_impl->currentPath.reset();
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
    delete m_impl;
    m_impl = nullptr;
  }
}

bool Canvas2D::initialize(RenderDevice &device, u32 width, u32 height) {
  if (tvg::Initializer::init(tvg::CanvasEngine::Sw, 0) !=
      tvg::Result::Success) {
    LOG_ERROR("Canvas2D: Failed to initialize ThorVG");
    return false;
  }

  m_width = width;
  m_height = height;

  m_impl->cpuBuffer.resize(width * height);
  std::memset(m_impl->cpuBuffer.data(), 0, width * height * sizeof(u32));

  m_impl->canvas = tvg::SwCanvas::gen();
  if (!m_impl->canvas) {
    LOG_ERROR("Canvas2D: Failed to create ThorVG canvas");
    return false;
  }

  m_impl->canvas->target(m_impl->cpuBuffer.data(), width, width, height,
                         tvg::SwCanvas::ARGB8888);

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
  texDesc.Format = TEX_FORMAT_BGRA8_UNORM;
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
  m_impl->canvas.reset();
  m_impl->pTexture.Release();
  m_impl->pSRV = nullptr;
  m_impl->cpuBuffer.clear();

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
  if (m_impl && m_impl->canvas) {
    m_impl->canvas->clear(true);
  }
  m_stateStack.reset(); // Reset to default state each frame
}

void Canvas2D::endFrame(RenderDevice &device) {
  if (!m_impl || !m_impl->canvas)
    return;

  m_impl->canvas->draw();
  m_impl->canvas->sync();

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

// ===== Target & Clearing =====
void Canvas2D::clear(u32 color) {
  if (!m_impl || !m_impl->canvas)
    return;

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, static_cast<float>(m_width),
                 static_cast<float>(m_height));

  u8 r, g, b, a;
  colorToRGBA(color, r, g, b, a);
  bg->fill(r, g, b, a);
  m_impl->canvas->push(std::move(bg));
}

// ===== State Stack =====
void Canvas2D::save() { m_stateStack.save(); }

void Canvas2D::restore() { m_stateStack.restore(); }

// ===== Transforms =====
void Canvas2D::resetTransform() {
  m_stateStack.current().transform = Transform2D::identity();
}

void Canvas2D::setTransform(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f) {
  m_stateStack.current().transform = {a, b, c, d, e, f};
}

void Canvas2D::translate(f32 x, f32 y) {
  m_stateStack.current().transform.translate(x, y);
}

void Canvas2D::rotate(f32 radians) {
  m_stateStack.current().transform.rotate(radians);
}

void Canvas2D::scale(f32 sx, f32 sy) {
  m_stateStack.current().transform.scale(sx, sy);
}

// ===== Global State =====
void Canvas2D::setGlobalAlpha(f32 alpha) {
  m_stateStack.current().globalAlpha = std::max(0.0f, std::min(1.0f, alpha));
}

void Canvas2D::setBlendMode(BlendMode mode) {
  m_stateStack.current().blendMode = mode;
}

// ===== Styles =====
void Canvas2D::setFillColor(u32 color) {
  m_stateStack.current().fillColor = color;
}

void Canvas2D::setStrokeColor(u32 color) {
  m_stateStack.current().strokeColor = color;
}

void Canvas2D::setLineWidth(f32 width) {
  m_stateStack.current().lineWidth = width;
}

void Canvas2D::setLineJoin(LineJoin join) {
  m_stateStack.current().lineJoin = join;
}

void Canvas2D::setLineCap(LineCap cap) { m_stateStack.current().lineCap = cap; }

void Canvas2D::setMiterLimit(f32 limit) {
  m_stateStack.current().miterLimit = limit;
}

// ===== Paths =====
void Canvas2D::beginPath() { m_impl->currentPath = tvg::Shape::gen(); }

void Canvas2D::closePath() {
  if (m_impl->currentPath) {
    m_impl->currentPath->close();
  }
}

void Canvas2D::moveTo(f32 x, f32 y) {
  if (m_impl->currentPath) {
    m_impl->currentPath->moveTo(x, y);
  }
}

void Canvas2D::lineTo(f32 x, f32 y) {
  if (m_impl->currentPath) {
    m_impl->currentPath->lineTo(x, y);
  }
}

void Canvas2D::quadTo(f32 cx, f32 cy, f32 x, f32 y) {
  if (m_impl->currentPath) {
    // ThorVG doesn't have quadTo directly, approximate with cubic
    // This is a simplification - proper implementation would use control points
    m_impl->currentPath->cubicTo(cx, cy, cx, cy, x, y);
  }
}

void Canvas2D::cubicTo(f32 c1x, f32 c1y, f32 c2x, f32 c2y, f32 x, f32 y) {
  if (m_impl->currentPath) {
    m_impl->currentPath->cubicTo(c1x, c1y, c2x, c2y, x, y);
  }
}

void Canvas2D::arc(f32 x, f32 y, f32 r, f32 startAngle, f32 endAngle,
                   bool /*ccw*/) {
  if (m_impl->currentPath) {
    // ThorVG appendArc is deprecated in v0.15
    // For now, use appendCircle for full circles, or approximate arcs
    // TODO: Implement proper arc using path commands
    (void)startAngle;
    (void)endAngle;
    m_impl->currentPath->appendCircle(x, y, r, r);
  }
}

void Canvas2D::rect(f32 x, f32 y, f32 w, f32 h) {
  if (m_impl->currentPath) {
    m_impl->currentPath->appendRect(x, y, w, h);
  }
}

// ===== Drawing =====
void Canvas2D::fill() {
  if (!m_impl || !m_impl->canvas || !m_impl->currentPath)
    return;

  const auto &state = m_stateStack.current();
  u8 r, g, b, a;
  colorToRGBA(state.fillColor, r, g, b, a);
  a = static_cast<u8>(a * state.globalAlpha);

  m_impl->currentPath->fill(r, g, b, a);
  m_impl->canvas->push(std::move(m_impl->currentPath));
  m_impl->currentPath = nullptr;
}

void Canvas2D::stroke() {
  if (!m_impl || !m_impl->canvas || !m_impl->currentPath)
    return;

  const auto &state = m_stateStack.current();
  u8 r, g, b, a;
  colorToRGBA(state.strokeColor, r, g, b, a);
  a = static_cast<u8>(a * state.globalAlpha);

  m_impl->currentPath->stroke(r, g, b, a);
  m_impl->currentPath->stroke(state.lineWidth);

  // Set stroke cap
  tvg::StrokeCap cap = tvg::StrokeCap::Butt;
  switch (state.lineCap) {
  case LineCap::Round:
    cap = tvg::StrokeCap::Round;
    break;
  case LineCap::Square:
    cap = tvg::StrokeCap::Square;
    break;
  default:
    break;
  }
  m_impl->currentPath->stroke(cap);

  // Set stroke join
  tvg::StrokeJoin join = tvg::StrokeJoin::Miter;
  switch (state.lineJoin) {
  case LineJoin::Round:
    join = tvg::StrokeJoin::Round;
    break;
  case LineJoin::Bevel:
    join = tvg::StrokeJoin::Bevel;
    break;
  default:
    break;
  }
  m_impl->currentPath->stroke(join);

  m_impl->canvas->push(std::move(m_impl->currentPath));
  m_impl->currentPath = nullptr;
}

void Canvas2D::fillRect(f32 x, f32 y, f32 w, f32 h) {
  if (!m_impl || !m_impl->canvas)
    return;

  const auto &state = m_stateStack.current();
  auto shape = tvg::Shape::gen();
  shape->appendRect(x, y, w, h);

  u8 r, g, b, a;
  colorToRGBA(state.fillColor, r, g, b, a);
  a = static_cast<u8>(a * state.globalAlpha);

  shape->fill(r, g, b, a);
  m_impl->canvas->push(std::move(shape));
}

void Canvas2D::strokeRect(f32 x, f32 y, f32 w, f32 h) {
  if (!m_impl || !m_impl->canvas)
    return;

  const auto &state = m_stateStack.current();
  auto shape = tvg::Shape::gen();
  shape->appendRect(x, y, w, h);

  u8 r, g, b, a;
  colorToRGBA(state.strokeColor, r, g, b, a);
  a = static_cast<u8>(a * state.globalAlpha);

  shape->stroke(r, g, b, a);
  shape->stroke(state.lineWidth);
  m_impl->canvas->push(std::move(shape));
}

void Canvas2D::clearRect(f32 x, f32 y, f32 w, f32 h) {
  if (!m_impl || !m_impl->canvas)
    return;

  auto shape = tvg::Shape::gen();
  shape->appendRect(x, y, w, h);
  shape->fill(0, 0, 0, 0); // Transparent
  m_impl->canvas->push(std::move(shape));
}

// ===== GPU Interface =====
void *Canvas2D::getShaderResourceView() {
  return m_impl ? m_impl->pSRV : nullptr;
}

bool Canvas2D::isValid() const {
  return m_impl && m_impl->canvas && m_impl->pTexture && m_impl->pSRV;
}

// ===== Images (§6.3.6) =====
u32 Canvas2D::loadImage(const char *path) {
  if (!m_impl || !path)
    return 0;

  auto pic = tvg::Picture::gen();
  if (!pic)
    return 0;

  // ThorVG loads from file path - in future integrate with VFS
  if (pic->load(path) != tvg::Result::Success) {
    LOG_ERROR("Canvas2D: Failed to load image: %s", path);
    return 0;
  }

  u32 handle = m_impl->nextImageHandle++;
  m_impl->images[handle] = std::move(pic);
  LOG_INFO("Canvas2D: Loaded image '%s' as handle %u", path, handle);
  return handle;
}

void Canvas2D::freeImage(u32 handle) {
  if (m_impl) {
    m_impl->images.erase(handle);
  }
}

bool Canvas2D::getImageSize(u32 handle, u32 &width, u32 &height) {
  if (!m_impl)
    return false;

  auto it = m_impl->images.find(handle);
  if (it == m_impl->images.end())
    return false;

  float w, h;
  it->second->size(&w, &h);
  width = static_cast<u32>(w);
  height = static_cast<u32>(h);
  return true;
}

void Canvas2D::drawImage(u32 handle, f32 x, f32 y) {
  if (!m_impl || !m_impl->canvas)
    return;

  auto it = m_impl->images.find(handle);
  if (it == m_impl->images.end())
    return;

  // Clone the picture for drawing
  auto pic = tvg::cast<tvg::Picture>(it->second->duplicate());
  if (!pic)
    return;

  pic->translate(x, y);

  // Apply global alpha
  const auto &state = m_stateStack.current();
  if (state.globalAlpha < 1.0f) {
    pic->opacity(static_cast<u8>(state.globalAlpha * 255));
  }

  m_impl->canvas->push(std::move(pic));
}

void Canvas2D::drawImageRect(u32 handle, i32 sx, i32 sy, i32 sw, i32 sh, f32 dx,
                             f32 dy, f32 dw, f32 dh) {
  if (!m_impl || !m_impl->canvas)
    return;

  auto it = m_impl->images.find(handle);
  if (it == m_impl->images.end())
    return;

  // Clone and scale
  auto pic = tvg::cast<tvg::Picture>(it->second->duplicate());
  if (!pic)
    return;

  // Set viewport for source rect (ThorVG doesn't directly support this)
  // For v0.1, we scale the whole image to dest rect
  (void)sx;
  (void)sy;
  (void)sw;
  (void)sh; // TODO: implement source rect

  float origW, origH;
  pic->size(&origW, &origH);
  pic->size(dw, dh);
  pic->translate(dx, dy);

  const auto &state = m_stateStack.current();
  if (state.globalAlpha < 1.0f) {
    pic->opacity(static_cast<u8>(state.globalAlpha * 255));
  }

  m_impl->canvas->push(std::move(pic));
}

// ===== Text (§6.3.8) =====
u32 Canvas2D::loadFont(const char *path, i32 sizePx) {
  if (!m_impl || !path)
    return 0;

  // Store font info for later use with tvg::Text
  u32 handle = m_impl->nextFontHandle++;
  m_impl->fonts[handle] = {std::string(path), sizePx};
  LOG_INFO("Canvas2D: Loaded font '%s' size %d as handle %u", path, sizePx,
           handle);
  return handle;
}

void Canvas2D::freeFont(u32 handle) {
  if (m_impl) {
    m_impl->fonts.erase(handle);
    if (m_impl->currentFontHandle == handle) {
      m_impl->currentFontHandle = 0;
    }
  }
}

void Canvas2D::setFont(u32 handle) {
  if (m_impl && m_impl->fonts.count(handle)) {
    m_impl->currentFontHandle = handle;
  }
}

void Canvas2D::setTextAlign(TextAlign align) {
  m_stateStack.current().textAlign = align;
}

void Canvas2D::setTextBaseline(TextBaseline baseline) {
  m_stateStack.current().textBaseline = baseline;
}

void Canvas2D::fillText(const char *text, f32 x, f32 y) {
  if (!m_impl || !m_impl->canvas || !text)
    return;
  if (m_impl->currentFontHandle == 0)
    return;

  auto it = m_impl->fonts.find(m_impl->currentFontHandle);
  if (it == m_impl->fonts.end())
    return;

  auto txt = tvg::Text::gen();
  if (!txt)
    return;

  // Load font into Text object
  if (txt->font(it->second.path.c_str(),
                static_cast<float>(it->second.sizePx)) !=
      tvg::Result::Success) {
    LOG_ERROR("Canvas2D: Failed to set font for text");
    return;
  }

  txt->text(text);
  txt->translate(x, y);

  // Set fill color
  const auto &state = m_stateStack.current();
  u8 r, g, b, a;
  colorToRGBA(state.fillColor, r, g, b, a);
  txt->fill(r, g, b);

  // Apply alpha via opacity
  u8 finalAlpha = static_cast<u8>(a * state.globalAlpha);
  if (finalAlpha < 255) {
    txt->opacity(finalAlpha);
  }

  m_impl->canvas->push(std::move(txt));
}

void Canvas2D::strokeText(const char *text, f32 x, f32 y) {
  // ThorVG Text doesn't directly support stroke text
  // For v0.1, fall back to fill text
  fillText(text, x, y);
}

} // namespace arcanee::render
