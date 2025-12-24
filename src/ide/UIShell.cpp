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

// Helper for recursion
void DrawTree(const FileNode &node,
              std::function<void(const std::string &)> onOpen) {
  if (node.isDirectory) {
    bool open = ImGui::TreeNode(node.name.c_str());
    if (open) {
      for (const auto &child : node.children) {
        DrawTree(child, onOpen);
      }
      ImGui::TreePop();
    }
  } else {
    if (ImGui::Selectable(node.name.c_str())) {
      onOpen(node.fullPath);
    }
  }
}

void UIShell::RenderPanes() {
  // Project Explorer
  if (ImGui::Begin("Project Explorer")) {
    DrawTree(m_projectSystem.GetRoot(), [this](const std::string &path) {
      Document *doc = nullptr;
      m_documentSystem.OpenDocument(path, &doc);
      if (doc)
        m_documentSystem.SetActiveDocument(doc);
    });
  }
  ImGui::End();

  // Editor Pane
  if (ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_None)) {
    Document *doc = m_documentSystem.GetActiveDocument();
    if (doc) {
      // Custom Editor Logic
      ImGui::BeginChild("TextEditor", ImVec2(0, 0), false,
                        ImGuiWindowFlags_HorizontalScrollbar);

      bool isFocused = ImGui::IsWindowFocused();
      if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        ImGui::SetWindowFocus();
        isFocused = true;
      }

      auto &buffer = doc->buffer;
      int totalLines = buffer.GetLineCount();
      float lineHeight = ImGui::GetTextLineHeight();
      float contentHeight = totalLines * lineHeight;

      // Dummy widget to effectively "reserve" space creates scrollbar
      ImGui::SetCursorPosY(contentHeight);
      ImGui::Dummy(ImVec2(0, 0));

      // Calculate visible range
      float scrollY = ImGui::GetScrollY();
      float windowHeight = ImGui::GetWindowHeight();
      int firstLine = (int)(scrollY / lineHeight);
      int linesVisible = (int)(windowHeight / lineHeight) + 2;
      int lastLine = std::min(totalLines, firstLine + linesVisible);

      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

      // Render visible lines
      for (int i = firstLine; i < lastLine; ++i) {
        std::string line = buffer.GetLine(i);
        ImGui::SetCursorPosY((float)i * lineHeight);
        ImGui::TextUnformatted(line.c_str());
      }

      ImGui::PopStyleVar();

      // Render Cursors
      const auto &cursors = buffer.GetCursors();
      auto *drawList = ImGui::GetWindowDrawList();
      ImVec2 winPos = ImGui::GetWindowPos();

      for (const auto &cursor : cursors) {
        int line = buffer.GetLineFromOffset(cursor.head);
        uint32_t lineStart = buffer.GetLineStart(line);
        int col = cursor.head - lineStart;

        // Calc pixel pos
        // Hack: assuming monospace char width 7px for now, or calc
        // Ideally: ImGui::CalcTextSize(line_substr).x
        std::string lineStr = buffer.GetLine(line);
        std::string subStr =
            lineStr.substr(0, std::min((size_t)col, lineStr.size()));
        float textWidth = ImGui::CalcTextSize(subStr.c_str()).x;

        ImVec2 cursorScreenPos =
            ImVec2(winPos.x - ImGui::GetScrollX() + textWidth,
                   winPos.y - scrollY + (float)line * lineHeight);

        drawList->AddLine(
            cursorScreenPos,
            ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineHeight),
            IM_COL32(255, 255, 255, 255));
      }

      // Input Handling
      if (isFocused) {
        auto &io = ImGui::GetIO();

        // Text Input
        for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
          char c = (char)io.InputQueueCharacters[i];
          if (c == 0)
            continue;
          std::string s(1, c);

          // TODO: Batch insert for multi-cursor
          // For MVP, just handle first cursor
          if (!cursors.empty()) {
            uint32_t pos = cursors[0].head;
            buffer.Insert(pos, s);
            buffer.SetCursor(pos + 1);
            doc->dirty = true;
          }
        }

        // Special Keys
        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
          if (!cursors.empty()) {
            uint32_t pos = cursors[0].head;
            buffer.Insert(pos, "\n");
            buffer.SetCursor(pos + 1);
            doc->dirty = true;
          }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
          if (!cursors.empty()) {
            uint32_t pos = cursors[0].head;
            if (pos > 0) {
              buffer.Delete(pos - 1, 1);
              buffer.SetCursor(pos - 1);
              doc->dirty = true;
            }
          }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
          if (!cursors.empty()) {
            uint32_t pos = cursors[0].head;
            if (pos > 0)
              buffer.SetCursor(pos - 1);
          }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
          if (!cursors.empty()) {
            uint32_t pos = cursors[0].head;
            if (pos < buffer.GetLength())
              buffer.SetCursor(pos + 1);
          }
        }
        // Up/Down TODO
      }

      ImGui::EndChild();
    } else {
      ImGui::Text("No open document");
    }
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
