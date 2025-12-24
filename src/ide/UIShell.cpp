#include "UIShell.h"
#include "imgui.h"
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace arcanee::ide {

// -----------------------------------------------------------------------------
// MainThreadQueue
// -----------------------------------------------------------------------------

void MainThreadQueue::Push(Job job) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_jobs.emplace_back(std::move(job));
}

void MainThreadQueue::DrainAll() {
  std::vector<Job> currentJobs;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_jobs.empty())
      return;
    currentJobs.swap(m_jobs);
  }

  for (const auto &job : currentJobs) {
    job();
  }
}

// -----------------------------------------------------------------------------
// UIShell
// -----------------------------------------------------------------------------

UIShell::UIShell(MainThreadQueue &queue) : m_queue(queue) {
  // Load layout/persistence?
}

UIShell::~UIShell() {
  // Save layout?
}

Status UIShell::RegisterCommand(std::string_view name, CommandFn fn,
                                CommandId *out) {
  // Placeholder registration
  return Status::Ok();
}

void UIShell::RenderFrame() {
  // 1. Drain Queue (Apply mutations)
  m_queue.DrainAll();

  // 2. Setup Dockspace
  RenderDockspace();

  // 3. Render Panes
  RenderPanes();

  // 4. Render Overlays (Command Palette)
  if (m_showCommandPalette) {
    RenderCommandPalette();
  }

  // Demo window for debugging
  // ImGui::ShowDemoWindow();
}

void UIShell::RenderDockspace() {
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  // ImGui::SetNextWindowViewport(viewport->ID); // Missing in standard ImGui

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;

#ifdef IMGUI_HAS_DOCK
  window_flags |= ImGuiWindowFlags_NoDocking;
#endif

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("ArcaneeDockSpace", nullptr, window_flags);
  ImGui::PopStyleVar(3);

#ifdef IMGUI_HAS_DOCK
  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
#endif

  // Main Menu Bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open Folder...")) { /* TODO */
      }
      if (ImGui::MenuItem("Exit")) { /* TODO via Runtime */
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Command Palette", "Ctrl+P")) {
        m_showCommandPalette = true;
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  ImGui::End();
}

void UIShell::RenderPanes() {
  // Placeholder Panes
  if (ImGui::Begin("Project Explorer")) {
    ImGui::Text("File Tree goes here...");
  }
  ImGui::End();

  if (ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_None)) {
    ImGui::Text("Code goes here...");
  }
  ImGui::End();

  if (ImGui::Begin("Preview")) {
    ImGui::Text("Runtime View goes here...");
  }
  ImGui::End();

  if (ImGui::Begin("Console")) {
    ImGui::Text("Logs/Task Output...");
  }
  ImGui::End();
}

void UIShell::RenderCommandPalette() {
  ImGui::OpenPopup("CommandPalette");

  // Center logic
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->Pos.y + 50),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(600, 0));

  if (ImGui::BeginPopupModal("CommandPalette", &m_showCommandPalette,
                             ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
    static char buf[256] = "";

    ImGui::PushItemWidth(-1);
    if (ImGui::IsWindowAppearing())
      ImGui::SetKeyboardFocusHere();
    ImGui::InputText("##Search", buf, IM_ARRAYSIZE(buf));
    ImGui::PopItemWidth();

    ImGui::BeginChild("Results", ImVec2(0, 200));
    if (ImGui::Selectable("Core: Reload Window")) {
      // Trigger action
      m_showCommandPalette = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndChild();

    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
      m_showCommandPalette = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

} // namespace arcanee::ide
