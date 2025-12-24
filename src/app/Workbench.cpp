#include "Workbench.h"
#include "common/Log.h"

// ImGui Headers
#include "backends/imgui_impl_sdl.h"
#include "imgui.h"

// Diligent Tools Headers
#include "Imgui/interface/ImGuiImplDiligent.hpp"

// Diligent Core Headers (for casting)
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

namespace arcanee::app {

using namespace Diligent;

struct Workbench::Impl {
  std::unique_ptr<ImGuiImplDiligent> pImguiDiligent;
  // ImguiImplSDL is not a class, it's a set of functions in imgui_impl_sdl.h
  // We don't need a unique_ptr for it.
};

Workbench::Workbench() : m_impl(std::make_unique<Impl>()) {}

Workbench::~Workbench() { shutdown(); }

bool Workbench::initialize(render::RenderDevice *device,
                           platform::Window *window) {
  LOG_INFO("Workbench: Initializing...");

  auto *pDevice = static_cast<IRenderDevice *>(device->getDevice());
  auto *pContext = static_cast<IDeviceContext *>(device->getContext());
  auto *pSwapChain = static_cast<ISwapChain *>(device->getSwapChain());

  if (!pDevice || !pContext || !pSwapChain) {
    LOG_ERROR("Workbench: RenderDevice not fully initialized");
    return false;
  }

  const auto &SCDesc = pSwapChain->GetDesc();

  // Create ImGui Diligent Implementation
  try {
    ImGuiDiligentCreateInfo CI(pDevice, SCDesc);
    m_impl->pImguiDiligent = std::make_unique<ImGuiImplDiligent>(CI);
  } catch (...) {
    LOG_ERROR("Workbench: Failed to create ImGuiImplDiligent");
    return false;
  }

  // Initialize ImGui SDL Backend
  // Since we use Diligent which handles its own rendering backend, we just need
  // basic SDL events. InitForVulkan/D3D/OpenGL usually sets up clipboard and
  // other stuff. InitForSDLRenderer is closest to generic SDL handling without
  // GL context? Actually InitForD3D just calls Init(window) and stores
  // bindings. Let's us InitForSDLRenderer(window, nullptr) if supported, or try
  // InitForVulkan(window) as it's generic window. imgui_impl_sdl.h ->
  // ImGui_ImplSDL2_InitForVulkan
  if (!ImGui_ImplSDL2_InitForVulkan(window->getNativeHandle())) {
    LOG_ERROR("Workbench: Failed to initialize ImGui SDL backend");
    return false;
  }

  LOG_INFO("Workbench: Initialized successfully");
  return true;
}

void Workbench::shutdown() {
  ImGui_ImplSDL2_Shutdown(); // Shutdown SDL backend
  m_impl->pImguiDiligent.reset();
}

void Workbench::update(double dt) {
  (void)dt;
  if (!m_visible)
    return;

  if (!m_impl->pImguiDiligent)
    return;

  // Actually ImguiImplDiligent::NewFrame simply calls ImGui::NewFrame?
  // No, it handles backend specific stuff.
  // Let's check signature. Usually:
  // NewFrame(Uint32 RenderSurfaceWidth, Uint32 RenderSurfaceHeight,
  // SURFACE_TRANSFORM SurfaceRotation)

  // For now, let's assume we can get dims from somewhere or allow ImguiImplSDL
  // to handle input. ImguiImplDiligent only handles rendering mostly.

  // Update ImGui SDL Backend
  ImGui_ImplSDL2_NewFrame();

  // We need current swapchain extents.
  // Accessing RenderDevice again? Easier to just use window size.
  // But Diligent backend needs to know target resolution for constant buffers.

  // Let's rely on ImGui::GetIO().DisplaySize
  auto &io = ImGui::GetIO();
  m_impl->pImguiDiligent->NewFrame(static_cast<Uint32>(io.DisplaySize.x),
                                   static_cast<Uint32>(io.DisplaySize.y),
                                   SURFACE_TRANSFORM_IDENTITY);

  ImGui::NewFrame();

  // Draw UI
  ImGui::ShowDemoWindow();

  ImGui::Render();
}

void Workbench::render(render::RenderDevice *device) {
  if (!m_visible)
    return;

  if (!m_impl->pImguiDiligent)
    return;

  auto *pContext = static_cast<IDeviceContext *>(device->getContext());
  m_impl->pImguiDiligent->Render(pContext);
}

bool Workbench::handleInput(const SDL_Event *event) {
  if (!m_visible)
    return false;

  // ProcessEvent returns true if handled
  const auto &io = ImGui::GetIO();
  ImGui_ImplSDL2_ProcessEvent(event);

  // If ImGui wants capture, return true
  if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
    return true;
  }

  return false;
}

} // namespace arcanee::app
