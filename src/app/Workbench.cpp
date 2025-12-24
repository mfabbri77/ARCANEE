#include "Workbench.h"
#include "Runtime.h"
#include "common/Log.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

// ImGui Headers
#include "backends/imgui_impl_sdl.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include <fstream>
#include <sstream>

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
                           platform::Window *window, Runtime *runtime) {
  LOG_INFO("Workbench: Initializing...");
  m_runtime = runtime;

  scanProjects();

  // Register Log Callback
  // Register Log Callback
  arcanee::Log::addCallback([this](const arcanee::LogMessage &msg) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    ConsoleLogEntry entry;
    entry.level = msg.level;
    entry.text = "[" + msg.timestamp + "] " + msg.message;
    m_logs.push_back(entry);
    if (m_logs.size() > 1000) {
      m_logs.erase(m_logs.begin());
    }
  });

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
  m_initialized = true;
  return true;
}

void Workbench::shutdown() {
  if (!m_initialized)
    return;
  m_initialized = false;

  ImGui_ImplSDL2_Shutdown();

  if (m_impl->pImguiDiligent) {
    m_impl->pImguiDiligent.reset();
  }
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

  // Draw Main Menu
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Project Browser", nullptr, m_showProjectBrowser)) {
        m_showProjectBrowser = !m_showProjectBrowser;
      }
      if (ImGui::MenuItem("Code Editor", nullptr, m_showCodeEditor)) {
        m_showCodeEditor = !m_showCodeEditor;
      }
      if (ImGui::MenuItem("Log Console", nullptr, m_showLogConsole)) {
        m_showLogConsole = !m_showLogConsole;
      }
      if (ImGui::MenuItem("Exit")) {
        // TODO: Request exit
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  // Draw Project Browser
  if (m_showProjectBrowser) {
    drawProjectBrowser();
  }

  // Draw Code Editor
  if (m_showCodeEditor) {
    drawCodeEditor();
  }

  // Draw Log Console
  if (m_showLogConsole) {
    drawLogConsole();
  }

  // Draw Demo (Optional toggle)
  // ImGui::ShowDemoWindow();

  ImGui::Render();
}

void Workbench::scanProjects() {
  m_projectList.clear();
  try {
    if (std::filesystem::exists(m_projectsDir) &&
        std::filesystem::is_directory(m_projectsDir)) {
      for (const auto &entry :
           std::filesystem::directory_iterator(m_projectsDir)) {
        if (entry.is_directory()) {
          m_projectList.push_back(entry.path().filename().string());
        }
      }
    }
    std::sort(m_projectList.begin(), m_projectList.end());
  } catch (const std::filesystem::filesystem_error &e) {
    LOG_ERROR("Workbench: Scan projects failed: %s", e.what());
  }
}

void Workbench::drawProjectBrowser() {
  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Project Browser", &m_showProjectBrowser)) {
    if (ImGui::Button("Scan Projects")) {
      scanProjects();
    }

    ImGui::SameLine();
    if (ImGui::Button("Create New...")) {
      ImGui::OpenPopup("Create New Project");
    }

    ImGui::Separator();

    if (ImGui::BeginChild("ProjectList")) {
      for (const auto &proj : m_projectList) {
        if (ImGui::Selectable(proj.c_str())) {
          // On single click, maybe select?
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          // Load Cartridge
          std::string path = m_projectsDir + "/" + proj;
          if (m_runtime) {
            m_runtime->loadCartridge(path);
          }
          openFile(path + "/main.nut");
          m_showCodeEditor = true;
        }
      }
    }
    ImGui::EndChild();

    // Create New Popup
    if (ImGui::BeginPopupModal("Create New Project", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      static char nameBuf[64] = "";
      ImGui::InputText("Project Name", nameBuf, IM_ARRAYSIZE(nameBuf));

      if (ImGui::Button("Create", ImVec2(120, 0))) {
        if (strlen(nameBuf) > 0) {
          std::string path = m_projectsDir + "/" + std::string(nameBuf);
          try {
            if (std::filesystem::create_directory(path)) {
              // Create main.nut
              // Needs file write. Basic std::ofstream.
            }
            scanProjects();
            ImGui::CloseCurrentPopup();
          } catch (...) {
          }
        }
      }
      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();
}

void Workbench::openFile(const std::string &path) {
  m_currentFilePath = path;
  m_codeEditorContent.clear();
  std::ifstream file(path);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    m_codeEditorContent = buffer.str();
    LOG_INFO("Workbench: Opened file '%s'", path.c_str());
  } else {
    LOG_ERROR("Workbench: Failed to open file '%s'", path.c_str());
  }
}

void Workbench::saveFile() {
  if (m_currentFilePath.empty())
    return;

  std::ofstream file(m_currentFilePath);
  if (file.is_open()) {
    file << m_codeEditorContent;
    LOG_INFO("Workbench: Saved file '%s'", m_currentFilePath.c_str());
    // Reload into Runtime?
    // m_runtime->reload(); // TODO
  } else {
    LOG_ERROR("Workbench: Failed to save file '%s'", m_currentFilePath.c_str());
  }
}

void Workbench::drawCodeEditor() {
  if (ImGui::Begin("Code Editor", &m_showCodeEditor,
                   ImGuiWindowFlags_MenuBar)) {
    if (ImGui::BeginMenuBar()) {
      if (ImGui::MenuItem("Save", "Ctrl+S")) {
        saveFile();
      }
      ImGui::EndMenuBar();
    }

    // InputTextMultiline with std::string
    ImGui::InputTextMultiline("##editor", &m_codeEditorContent,
                              ImVec2(-FLT_MIN, -FLT_MIN),
                              ImGuiInputTextFlags_AllowTabInput);
  }
  ImGui::End();
}

void Workbench::drawLogConsole() {
  if (ImGui::Begin("Log Console", &m_showLogConsole)) {
    if (ImGui::Button("Clear")) {
      std::lock_guard<std::mutex> lock(m_logMutex);
      m_logs.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScrollLog);

    ImGui::Separator();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    {
      std::lock_guard<std::mutex> lock(m_logMutex);
      for (const auto &log : m_logs) {
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        if (log.level == arcanee::LogLevel::Error ||
            log.level == arcanee::LogLevel::Fatal)
          color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        else if (log.level == arcanee::LogLevel::Warning)
          color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
        else if (log.level == arcanee::LogLevel::Debug)
          color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(log.text.c_str());
        ImGui::PopStyleColor();
      }
    }

    if (m_autoScrollLog && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
      ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
  }
  ImGui::End();
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
