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
  // Initialize services
  m_parseService.Initialize();
  m_lspClient.Initialize();

  // Initialize timeline store in project data directory
  // MVP: Use temp path, real implementation would use project-specific path
  m_timelineStore.Initialize("/tmp/arcanee_timeline.db");

  // Load tasks from project root (if available)
  m_taskRunner.LoadTasks(".");
}

UIShell::~UIShell() {
  m_parseService.Shutdown();
  m_lspClient.Shutdown();
  m_timelineStore.Shutdown();
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
      // Trigger parse if dirty or first open?
      // MVP: Trigger parse on every frame dirty check?
      // Better: DocumentSystem tells us, or we poll revision.
      // Let's assume on keypress input we trigger it.

      // ... (rendering logic below)
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
      // Render visible lines and Selections
      // This is a simplified single-layer pass. Better: Render background
      // selection, then text. We'll render text first, then simple cursors.
      // Selection requires calculation.

      // Fetch Highlights
      const ParseResult *parseRes = m_parseService.GetHighlights(doc->path);

      // 1. Render Text
      for (int i = firstLine; i < lastLine; ++i) {
        std::string line = buffer.GetLine(i);
        uint32_t lineStart = buffer.GetLineStart(i); // Offset

        ImGui::SetCursorPosY((float)i * lineHeight);

        // Standard render
        // ImGui::TextUnformatted(line.c_str());

        // Highlighted render
        if (parseRes && !parseRes->highlights.empty()) {
          float x = 0;
          size_t currentPos = 0;
          while (currentPos < line.size()) {
            uint32_t absPos = lineStart + currentPos;
            // Find span covering absPos
            // Optimization: Use interval tree or sorted vector search.
            // For MVP: Linear scan or just default color if no span.
            // We need a proper span iterator for the line.

            uint32_t color = IM_COL32(220, 220, 220, 255); // Default Text
            size_t chunkLen = line.size() - currentPos;

            for (const auto &span : parseRes->highlights) {
              if (absPos >= span.startByte && absPos < span.endByte) {
                color = span.color;
                chunkLen = std::min((uint32_t)chunkLen, span.endByte - absPos);
                break;
              } else if (span.startByte > absPos) {
                chunkLen =
                    std::min((uint32_t)chunkLen, span.startByte - absPos);
                // We can break if sorted? Assumed sorted.
              }
            }

            std::string chunk = line.substr(currentPos, chunkLen);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(chunk.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);

            currentPos += chunkLen;
          }
          ImGui::NewLine();
        } else {
          ImGui::TextUnformatted(line.c_str());
        }
      }
      ImGui::PopStyleVar();

      // 2. Render Cursors & Selection
      const auto &cursors = buffer.GetCursors();
      auto *drawList = ImGui::GetWindowDrawList();
      ImVec2 winPos = ImGui::GetWindowPos();
      float scrollX = ImGui::GetScrollX();

      for (const auto &cursor : cursors) {
        // Calculate Head Position
        int headLine = buffer.GetLineFromOffset(cursor.head);
        uint32_t headLineStart = buffer.GetLineStart(headLine);
        int headCol = cursor.head - headLineStart;

        std::string headLineStr = buffer.GetLine(headLine);
        std::string headSub = headLineStr.substr(
            0, std::min((size_t)headCol, headLineStr.size()));
        float headX = ImGui::CalcTextSize(headSub.c_str()).x;
        ImVec2 headScreenPos =
            ImVec2(winPos.x - scrollX + headX,
                   winPos.y - scrollY + (float)headLine * lineHeight);

        // Draw Cursor
        drawList->AddLine(headScreenPos,
                          ImVec2(headScreenPos.x, headScreenPos.y + lineHeight),
                          IM_COL32(255, 255, 255, 255));

        // Draw Selection (if anchor != head)
        if (cursor.anchor != cursor.head) {
          // Simplified: Only support single-line selection visual correctly
          // for now or simple range Full multi-line selection rendering
          // requires iterating lines between anchor and head.
          int anchorLine = buffer.GetLineFromOffset(cursor.anchor);
          uint32_t anchorLineStart = buffer.GetLineStart(anchorLine);
          int anchorCol = cursor.anchor - anchorLineStart;

          // Normalize start/end
          uint32_t start = std::min(cursor.head, cursor.anchor);
          uint32_t end = std::max(cursor.head, cursor.anchor);

          int startL = buffer.GetLineFromOffset(start);
          int endL = buffer.GetLineFromOffset(end);

          for (int l = startL; l <= endL; ++l) {
            uint32_t ls = buffer.GetLineStart(l);
            std::string lStr = buffer.GetLine(l);

            int colStart = (l == startL) ? (start - ls) : 0;
            int colEnd = (l == endL)
                             ? (end - ls)
                             : (int)lStr.size(); // TODO: +1 for newline visual?

            std::string preStr =
                lStr.substr(0, std::min((size_t)colStart, lStr.size()));
            std::string selStr =
                lStr.substr(std::min((size_t)colStart, lStr.size()),
                            std::max(0, colEnd - colStart));

            float x1 = ImGui::CalcTextSize(preStr.c_str()).x;
            float w = ImGui::CalcTextSize(selStr.c_str()).x;
            if (w == 0)
              w = 5; // Width for empty line selection

            ImVec2 pMin(winPos.x - scrollX + x1,
                        winPos.y - scrollY + (float)l * lineHeight);
            ImVec2 pMax(pMin.x + w, pMin.y + lineHeight);

            drawList->AddRectFilled(
                pMin, pMax, IM_COL32(0, 120, 215, 100)); // Transparent Blue
          }
        }
      }

      // Input Handling
      if (isFocused) {
        auto &io = ImGui::GetIO();
        bool ctrl = io.KeyCtrl;
        bool shift = io.KeyShift;

        if (!io.InputQueueCharacters.empty())
          buffer.BeginBatch();

        // Text Input
        for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
          char c = (char)io.InputQueueCharacters[i];
          if (c == 0)
            continue;
          std::string s(1, c);

          if (!cursors.empty()) {
            // TODO: Multi-cursor batch
            uint32_t pos = cursors[0].head;
            // If selection, delete first
            if (cursors[0].head != cursors[0].anchor) {
              uint32_t start = std::min(cursors[0].head, cursors[0].anchor);
              uint32_t len =
                  std::max(cursors[0].head, cursors[0].anchor) - start;
              buffer.Delete(start, len);
              pos = start;
              buffer.SetCursor(start);
            }
            buffer.Insert(pos, s);
            buffer.SetCursor(pos + 1);
            doc->dirty = true;
            m_parseService.UpdateDocument(doc, buffer.GetAllText(),
                                          0); // Trigger Parse
          }
        }
        if (!io.InputQueueCharacters.empty())
          buffer.EndBatch();

        // Shortcuts
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
          if (shift)
            buffer.Redo(); // Ctrl+Shift+Z
          else
            buffer.Undo();
          doc->dirty = true; // Heuristic
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
          buffer.Redo();
          doc->dirty = true;
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
          Cursor c;
          c.anchor = 0;
          c.head = buffer.GetLength();
          buffer.SetCursors({c});
        }

        // Clipboard Copy
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
          std::string allText;
          for (const auto &c : cursors) {
            if (c.head != c.anchor) {
              uint32_t start = std::min(c.head, c.anchor);
              uint32_t len = std::max(c.head, c.anchor) - start;
              if (!allText.empty())
                allText += "\n";
              allText += buffer.GetText(start, len);
            }
          }
          if (!allText.empty()) {
            ImGui::SetClipboardText(allText.c_str());
          }
        }

        // Clipboard Cut
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
          std::string allText;
          // Capture first
          for (const auto &c : cursors) {
            if (c.head != c.anchor) {
              uint32_t start = std::min(c.head, c.anchor);
              uint32_t len = std::max(c.head, c.anchor) - start;
              if (!allText.empty())
                allText += "\n";
              allText += buffer.GetText(start, len);
            }
          }
          if (!allText.empty()) {
            ImGui::SetClipboardText(allText.c_str());

            buffer.BeginBatch();
            // Naive single-cursor cut for now or multi-cursor if no overlap
            // Issue: Deleting affects subsequent indices.
            // MVP hack: Just delete first cursor's selection to avoid
            // corruption.
            // TODO: Sort cursors descending to handle multi cut safely.
            if (cursors.size() > 0 && cursors[0].head != cursors[0].anchor) {
              uint32_t start = std::min(cursors[0].head, cursors[0].anchor);
              uint32_t len =
                  std::max(cursors[0].head, cursors[0].anchor) - start;
              buffer.Delete(start, len);
              buffer.SetCursor(start);
              doc->dirty = true;
            }
            buffer.EndBatch();
          }
        }

        // Clipboard Paste
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
          const char *clip = ImGui::GetClipboardText();
          if (clip && cursors.size() > 0) {
            buffer.BeginBatch(); // Treat paste as atomic
            uint32_t pos = cursors[0].head;
            // Replace selection if exists
            if (cursors[0].head != cursors[0].anchor) {
              uint32_t start = std::min(cursors[0].head, cursors[0].anchor);
              uint32_t len =
                  std::max(cursors[0].head, cursors[0].anchor) - start;
              buffer.Delete(start, len);
              pos = start;
            }
            buffer.Insert(pos, clip);
            buffer.SetCursor(pos + strlen(clip));
            buffer.EndBatch();
            doc->dirty = true;
          }
        }
        // Navigation
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
          if (!cursors.empty()) {
            uint32_t pos = cursors[0].head;
            if (pos > 0)
              pos--;

            if (shift) {
              Cursor c = cursors[0];
              c.head = pos;
              std::vector<Cursor> newCs = {c};
              buffer.SetCursors(newCs);
            } else {
              buffer.SetCursor(pos);
            }
          }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
          if (!cursors.empty()) {
            uint32_t pos = cursors[0].head;
            if (pos < buffer.GetLength())
              pos++;

            if (shift) {
              Cursor c = cursors[0];
              c.head = pos;
              std::vector<Cursor> newCs = {c};
              buffer.SetCursors(newCs);
            } else {
              buffer.SetCursor(pos);
            }
          }
        }
        // Backspace
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
          if (!cursors.empty()) {
            buffer.BeginBatch();
            // Handle selection delete
            if (cursors[0].head != cursors[0].anchor) {
              uint32_t start = std::min(cursors[0].head, cursors[0].anchor);
              uint32_t len =
                  std::max(cursors[0].head, cursors[0].anchor) - start;
              buffer.Delete(start, len);
              buffer.SetCursor(start);
            } else {
              uint32_t pos = cursors[0].head;
              if (pos > 0) {
                buffer.Delete(pos - 1, 1);
                buffer.SetCursor(pos - 1);
              }
            }
            buffer.EndBatch();
            doc->dirty = true;
          }
        }
        // Enter
        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
          if (!cursors.empty()) {
            buffer.BeginBatch();
            uint32_t pos = cursors[0].head;
            buffer.Insert(pos, "\n");
            buffer.SetCursor(pos + 1);
            buffer.EndBatch();
            doc->dirty = true;
          }
        }
      }

      ImGui::EndChild();
    } else {
      ImGui::Text("No open document");
    }
  }
  ImGui::End();

  if (ImGui::Begin("Preview")) {
    Document *doc = m_documentSystem.GetActiveDocument();

    if (ImGui::Button("Run Preview")) {
      if (doc) {
        // MVP: Placeholder for runtime preview integration
        // In full implementation, this would launch the script in embedded
        // runtime
        ImGui::OpenPopup("PreviewRunning");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
      // Stop preview execution
    }

    ImGui::Separator();

    // Preview area
    ImGui::BeginChild("PreviewArea", ImVec2(0, 0), true);
    ImGui::Text("Runtime Preview");
    ImGui::TextDisabled("(Preview integration pending full Runtime binding)");
    if (doc) {
      ImGui::Text("Current file: %s", doc->path.c_str());
    }
    ImGui::EndChild();
  }
  ImGui::End();

  if (ImGui::Begin("Console")) {
    // Task dropdown
    const auto &tasks = m_taskRunner.GetTasks();
    if (!tasks.empty()) {
      if (ImGui::BeginCombo("Task",
                            m_selectedTaskIndex >= 0
                                ? tasks[m_selectedTaskIndex].name.c_str()
                                : "Select Task")) {
        for (int i = 0; i < (int)tasks.size(); ++i) {
          bool selected = (m_selectedTaskIndex == i);
          if (ImGui::Selectable(tasks[i].name.c_str(), selected)) {
            m_selectedTaskIndex = i;
          }
        }
        ImGui::EndCombo();
      }

      ImGui::SameLine();
      if (m_taskRunner.IsRunning()) {
        if (ImGui::Button("Cancel")) {
          m_taskRunner.Cancel();
        }
      } else {
        if (ImGui::Button("Run") && m_selectedTaskIndex >= 0) {
          m_taskRunner.RunTask(tasks[m_selectedTaskIndex].name);
        }
      }
    } else {
      ImGui::Text("No tasks.toml found");
    }

    ImGui::Separator();

    // Output display
    ImGui::BeginChild("TaskOutput", ImVec2(0, 0), true);
    auto output = m_taskRunner.GetOutput();
    for (const auto &line : output) {
      // Color errors red, warnings yellow
      if (line.find("error") != std::string::npos ||
          line.find("Error") != std::string::npos) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", line.c_str());
      } else if (line.find("warning") != std::string::npos ||
                 line.find("Warning") != std::string::npos) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "%s", line.c_str());
      } else {
        ImGui::TextUnformatted(line.c_str());
      }
    }
    // Auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10) {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
  }
  ImGui::End();

  RenderSearchPane();

  // Timeline Pane
  if (ImGui::Begin("Local History")) {
    Document *doc = m_documentSystem.GetActiveDocument();
    if (doc) {
      auto history = m_timelineStore.GetHistory(doc->path);

      if (history.empty()) {
        ImGui::Text("No history for this file.");
      } else {
        ImGui::Text("%zu snapshots", history.size());
        ImGui::Separator();

        for (const auto &entry : history) {
          // Format timestamp
          char timeBuf[64];
          std::time_t t = (std::time_t)entry.timestamp;
          std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S",
                        std::localtime(&t));

          std::string label = std::string(timeBuf) + " [" + entry.trigger +
                              "] (" + std::to_string(entry.originalSize) +
                              " bytes)";
          if (ImGui::Selectable(label.c_str())) {
            // Restore snapshot
            std::string content = m_timelineStore.RestoreSnapshot(entry.id);
            if (!content.empty()) {
              doc->buffer.Load(content);
              doc->dirty = true;
            }
          }
        }
      }
    } else {
      ImGui::Text("No document open.");
    }
  }
  ImGui::End();

  // Debugger Control Bar
  if (ImGui::Begin("Debugger")) {
    DebugState state = m_dapClient.GetState();

    if (state == DebugState::Disconnected) {
      Document *doc = m_documentSystem.GetActiveDocument();
      if (doc && ImGui::Button("Launch Debug")) {
        m_dapClient.Launch(doc->path);
      }
    } else if (state == DebugState::Stopped) {
      if (ImGui::Button("Continue"))
        m_dapClient.Continue();
      ImGui::SameLine();
      if (ImGui::Button("Step In"))
        m_dapClient.StepIn();
      ImGui::SameLine();
      if (ImGui::Button("Step Over"))
        m_dapClient.StepOver();
      ImGui::SameLine();
      if (ImGui::Button("Step Out"))
        m_dapClient.StepOut();
      ImGui::SameLine();
      if (ImGui::Button("Stop"))
        m_dapClient.Stop();
    } else if (state == DebugState::Running) {
      if (ImGui::Button("Pause"))
        m_dapClient.Pause();
      ImGui::SameLine();
      if (ImGui::Button("Stop"))
        m_dapClient.Stop();
    } else {
      if (ImGui::Button("Disconnect"))
        m_dapClient.Disconnect();
    }

    ImGui::Separator();

    // Call Stack
    ImGui::Text("Call Stack:");
    auto stack = m_dapClient.GetCallStack();
    for (const auto &frame : stack) {
      std::string label = frame.name + " (" + std::to_string(frame.line) + ")";
      if (ImGui::Selectable(label.c_str())) {
        // Navigate to frame location
        Document *doc = nullptr;
        if (m_documentSystem.OpenDocument(frame.file, &doc).ok()) {
          m_documentSystem.SetActiveDocument(doc);
          doc->buffer.SetCursor(doc->buffer.GetLineStart(frame.line - 1));
        }
      }
    }

    ImGui::Separator();

    // Variables
    ImGui::Text("Locals:");
    auto locals = m_dapClient.GetLocals(0);
    for (const auto &var : locals) {
      ImGui::Text("  %s = %s", var.name.c_str(), var.value.c_str());
    }
  }
  ImGui::End();

  // Breakpoints pane
  if (ImGui::Begin("Breakpoints")) {
    auto breakpoints = m_dapClient.GetBreakpoints();
    for (const auto &bp : breakpoints) {
      std::string label = std::filesystem::path(bp.file).filename().string() +
                          ":" + std::to_string(bp.line);
      bool enabled = bp.enabled;
      if (ImGui::Checkbox(("##bp" + std::to_string(bp.id)).c_str(), &enabled)) {
        m_dapClient.ToggleBreakpoint(bp.file, bp.line);
      }
      ImGui::SameLine();
      ImGui::Text("%s", label.c_str());
    }

    // Add breakpoint via current line
    Document *doc = m_documentSystem.GetActiveDocument();
    if (doc && ImGui::Button("Add at Cursor")) {
      auto cursors = doc->buffer.GetCursors();
      if (!cursors.empty()) {
        int line = doc->buffer.GetLineFromOffset(cursors[0].head) + 1;
        m_dapClient.SetBreakpoint(doc->path, line);
      }
    }
  }
  ImGui::End();

  // Problems pane
  if (ImGui::Begin("Problems")) {
    auto diagnostics = m_lspClient.GetDiagnostics();

    if (diagnostics.empty()) {
      ImGui::Text("No problems");
    } else {
      ImGui::Text("%zu issues", diagnostics.size());
      ImGui::Separator();

      for (const auto &diag : diagnostics) {
        ImVec4 color;
        const char *icon;
        switch (diag.severity) {
        case DiagnosticSeverity::Error:
          color = ImVec4(1, 0.3f, 0.3f, 1);
          icon = "[E]";
          break;
        case DiagnosticSeverity::Warning:
          color = ImVec4(1, 1, 0.3f, 1);
          icon = "[W]";
          break;
        case DiagnosticSeverity::Information:
          color = ImVec4(0.3f, 0.7f, 1, 1);
          icon = "[I]";
          break;
        default:
          color = ImVec4(0.7f, 0.7f, 0.7f, 1);
          icon = "[H]";
          break;
        }

        std::string label =
            std::string(icon) + " " +
            std::filesystem::path(diag.file).filename().string() + ":" +
            std::to_string(diag.line) + " - " + diag.message;

        ImGui::TextColored(color, "%s", label.c_str());
        if (ImGui::IsItemClicked()) {
          Document *doc = nullptr;
          if (m_documentSystem.OpenDocument(diag.file, &doc).ok()) {
            m_documentSystem.SetActiveDocument(doc);
            doc->buffer.SetCursor(doc->buffer.GetLineStart(diag.line - 1));
          }
        }
      }
    }
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

void UIShell::RenderSearchPane() {
  if (ImGui::Begin("Search")) {
    // Query Input
    static char queryBuf[256] = "";
    if (ImGui::InputText("Query", queryBuf, IM_ARRAYSIZE(queryBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      m_searchQuery = queryBuf;
      m_searchService.StartSearch(m_projectSystem.GetRoot().fullPath,
                                  m_searchQuery, m_searchRegex,
                                  m_searchCaseSensitive);
    }

    ImGui::Checkbox("Match Case", &m_searchCaseSensitive);
    ImGui::SameLine();
    ImGui::Checkbox("Regex", &m_searchRegex);

    if (ImGui::Button("Find in Files")) {
      m_searchQuery = queryBuf;
      m_searchService.StartSearch(m_projectSystem.GetRoot().fullPath,
                                  m_searchQuery, m_searchRegex,
                                  m_searchCaseSensitive);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      m_searchService.CancelSearch();
    }

    ImGui::Separator();

    // Search Results
    SearchResult res = m_searchService.GetResults(); // Copy
    if (!res.query.empty()) {
      if (res.complete)
        ImGui::Text("Search complete. %zu matches.", res.matches.size());
      else
        ImGui::Text("Searching... %zu matches found so far.",
                    res.matches.size());

      ImGui::BeginChild("SearchResults");

      // Group by File for better UI
      std::string lastFile = "";
      for (const auto &match : res.matches) {
        if (match.filePath != lastFile) {
          ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s",
                             std::filesystem::path(match.filePath)
                                 .filename()
                                 .string()
                                 .c_str());
          lastFile = match.filePath;
          // Tooltip full path?
        }

        std::string label =
            std::to_string(match.lineNumber) + ": " + match.lineContent;
        if (ImGui::Selectable(label.c_str())) {
          Document *doc = nullptr;
          if (m_documentSystem.OpenDocument(match.filePath, &doc).ok()) {
            m_documentSystem.SetActiveDocument(doc);
            // Jump to line
            uint32_t lineStart =
                doc->buffer.GetLineStart(match.lineNumber - 1); // 0-indexed
            doc->buffer.SetCursor(lineStart);
            // TODO: Center view
          }
        }
      }
      ImGui::EndChild();
    }
  }
  ImGui::End();
}

} // namespace arcanee::ide
