#include "UIShell.h"
#include "imgui.h"
#include "imgui_internal.h" // Required for DockBuilder API
#include <algorithm>
#include <filesystem>
#include <fstream>
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

  // 1b. Global Debug Keybindings
  {
    auto &io = ImGui::GetIO();
    bool shift = io.KeyShift;
    bool ctrl = io.KeyCtrl;
    DebugState state = m_dapClient.GetState();

    // Ctrl+R: Run Without Debugging
    if ((ImGui::IsKeyPressed(ImGuiKey_R) && ctrl && !shift)) {
      bool isLoaded = m_isCartridgeLoadedFn && m_isCartridgeLoadedFn();
      bool isRunning = m_isCartridgeRunningFn && m_isCartridgeRunningFn();
      if (isLoaded && !isRunning && state == DebugState::Disconnected) {
        if (m_startCartridgeFn)
          m_startCartridgeFn();
        if (m_resumeRuntimeFn)
          m_resumeRuntimeFn();
      }
    }

    // F5: Start Debugging / Continue
    if (ImGui::IsKeyPressed(ImGuiKey_F5) && !shift && !ctrl) {
      if (state == DebugState::Disconnected) {
        bool isLoaded = m_isCartridgeLoadedFn && m_isCartridgeLoadedFn();
        bool isRunning = m_isCartridgeRunningFn && m_isCartridgeRunningFn();

        if (!isRunning) { // Only start debug if not already running natively
          m_showDebugger = true;
          m_showBreakpoints = true;
          // Connect DapClient to ScriptEngine if available
          if (m_getScriptEngineFn) {
            m_dapClient.SetScriptEngine(m_getScriptEngineFn());
          }

          Document *doc = m_documentSystem.GetActiveDocument();
          std::string launchPath = doc ? doc->path : "";
          m_dapClient.Launch(launchPath);

          if (m_pauseRuntimeFn)
            m_pauseRuntimeFn(); // Pause preview when debugging starts
        }
      } else if (state == DebugState::Stopped) {
        if (m_resumeRuntimeFn)
          m_resumeRuntimeFn(); // Resume preview when continuing
        m_dapClient.Continue();
      }
    }

    // Shift+F5: Stop Debugging
    if (ImGui::IsKeyPressed(ImGuiKey_F5) && shift && !ctrl) {
      if (state != DebugState::Disconnected) {
        m_dapClient.Stop();
        if (m_resumeRuntimeFn)
          m_resumeRuntimeFn(); // Resume preview when stopping debug
      }
    }

    // F6: Pause
    if (ImGui::IsKeyPressed(ImGuiKey_F6)) {
      if (state == DebugState::Running) {
        m_dapClient.Pause();
        if (m_pauseRuntimeFn)
          m_pauseRuntimeFn();
      }
    }

    // F9: Toggle Breakpoint at cursor
    if (ImGui::IsKeyPressed(ImGuiKey_F9)) {
      Document *doc = m_documentSystem.GetActiveDocument();
      if (doc) {
        auto cursors = doc->buffer.GetCursors();
        if (!cursors.empty()) {
          int line = doc->buffer.GetLineFromOffset(cursors[0].head) + 1;
          m_dapClient.ToggleBreakpoint(doc->path, line);
        }
      }
    }

    // F10: Step Over
    if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
      if (state == DebugState::Stopped) {
        m_dapClient.StepOver();
      }
    }

    // F11: Step Into
    if (ImGui::IsKeyPressed(ImGuiKey_F11) && !shift) {
      if (state == DebugState::Stopped) {
        m_dapClient.StepIn();
      }
    }

    // Shift+F11: Step Out
    if (ImGui::IsKeyPressed(ImGuiKey_F11) && shift) {
      if (state == DebugState::Stopped) {
        m_dapClient.StepOut();
      }
    }
  }

  // 2. Setup Dockspace
  RenderDockspace();

  // 3. Render Panes
  RenderPanes();

  // 4. Render Search Pane (visibility controlled)
  if (m_showSearch) {
    RenderSearchPane();
  }

  // 5. Render Output/Console Panes (visibility controlled)
  if (m_showOutput) {
    RenderOutputPane();
  }
  if (m_showConsole) {
    RenderConsolePane();
  }

  // 6. Render Preview Pane (visibility controlled)
  if (m_showPreview) {
    RenderPreviewPane();
  }

  // 7. Render Overlays (Command Palette, Folder Dialog, New Project Dialog)
  if (m_showCommandPalette) {
    RenderCommandPalette();
  }
  if (m_showFolderDialog) {
    RenderFolderDialog();
  }
  if (m_showNewProjectDialog) {
    RenderNewProjectDialog();
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

  // Setup default layout on first run
  static bool first_run = true;
  if (first_run) {
    first_run = false;
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    // IDE Layout:
    // +-------------------+---------------------------+----------------+
    // | Explorer (15%)    |  Editor (55%)             | Preview (30%)  |
    // | Breakpoints       |                           |                |
    // +-------------------+---------------------------+----------------+
    //                     |  Console/Output/Problems/Debugger (20%)    |
    //                     +--------------------------------------------+

    // Step 1: Split left sidebar (15%)
    ImGuiID dock_left, dock_main;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.15f, &dock_left,
                                &dock_main);

    // Step 2: Split right preview pane (30% of remaining)
    ImGuiID dock_center_area, dock_right;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.35f, &dock_right,
                                &dock_center_area);

    // Step 3: Split bottom panel (20% of center area)
    ImGuiID dock_center, dock_bottom;
    ImGui::DockBuilderSplitNode(dock_center_area, ImGuiDir_Down, 0.25f,
                                &dock_bottom, &dock_center);

    // Step 4: Split left sidebar for Breakpoints (50% of left)
    ImGuiID dock_left_top, dock_left_bottom;
    ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.4f,
                                &dock_left_bottom, &dock_left_top);

    // Dock windows to their positions
    ImGui::DockBuilderDockWindow("Project Explorer", dock_left_top);
    ImGui::DockBuilderDockWindow("Search", dock_left_top);
    ImGui::DockBuilderDockWindow("Breakpoints", dock_left_bottom);
    ImGui::DockBuilderDockWindow("Editor", dock_center);
    ImGui::DockBuilderDockWindow("Preview", dock_right);
    ImGui::DockBuilderDockWindow("Console", dock_bottom);
    ImGui::DockBuilderDockWindow("Output", dock_bottom);
    ImGui::DockBuilderDockWindow("Problems", dock_bottom);
    ImGui::DockBuilderDockWindow("Debugger", dock_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
#endif

  // Main Menu Bar
  if (ImGui::BeginMenuBar()) {
    // === FILE MENU ===
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Project...")) {
        m_newProjectName[0] = '\0';
        m_newProjectError.clear();
        m_showNewProjectDialog = true;
      }
      if (ImGui::MenuItem("Open Folder...", "Ctrl+O")) {
        // Start from samples/ directory if it exists
        std::filesystem::path current = std::filesystem::current_path();
        std::filesystem::path samplesPath = current / "samples";
        std::filesystem::path parentSamplesPath =
            current.parent_path() / "samples";

        if (std::filesystem::exists(samplesPath) &&
            std::filesystem::is_directory(samplesPath)) {
          m_folderDialogPath = samplesPath.string();
        } else if (std::filesystem::exists(parentSamplesPath) &&
                   std::filesystem::is_directory(parentSamplesPath)) {
          m_folderDialogPath = parentSamplesPath.string();
        } else {
          m_folderDialogPath = current.string();
        }
        m_folderDialogError.clear();
        m_showFolderDialog = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Save", "Ctrl+S")) {
        Document *doc = m_documentSystem.GetActiveDocument();
        if (doc)
          m_documentSystem.SaveDocument(doc);
      }
      if (ImGui::MenuItem("Save All", "Ctrl+Shift+S")) {
        // TODO: Save all open documents
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
        if (m_requestExitFn)
          m_requestExitFn();
      }
      ImGui::EndMenu();
    }

    // === EDIT MENU ===
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
        Document *doc = m_documentSystem.GetActiveDocument();
        if (doc)
          doc->buffer.Undo();
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
        Document *doc = m_documentSystem.GetActiveDocument();
        if (doc)
          doc->buffer.Redo();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Find in Files", "Ctrl+Shift+F")) {
        m_showSearch = true;
      }
      if (ImGui::MenuItem("Command Palette", "Ctrl+P")) {
        m_showCommandPalette = true;
      }
      ImGui::EndMenu();
    }

    // === VIEW MENU ===
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Explorer", nullptr, &m_showExplorer);
      ImGui::MenuItem("Search", "Ctrl+Shift+F", &m_showSearch);
      ImGui::MenuItem("Problems", nullptr, &m_showProblems);
      ImGui::MenuItem("Output", nullptr, &m_showOutput);
      ImGui::MenuItem("Console", nullptr, &m_showConsole);
      ImGui::Separator();
      ImGui::MenuItem("Debugger", nullptr, &m_showDebugger);
      ImGui::MenuItem("Breakpoints", nullptr, &m_showBreakpoints);
      ImGui::MenuItem("Local History", nullptr, &m_showLocalHistory);
      ImGui::Separator();
      if (ImGui::MenuItem("Command Palette", "Ctrl+P")) {
        m_showCommandPalette = true;
      }
      ImGui::EndMenu();
    }

    // === RUN MENU ===
    if (ImGui::BeginMenu("Run")) {
      DebugState state = m_dapClient.GetState();
      bool isRunning = m_isCartridgeRunningFn && m_isCartridgeRunningFn();
      bool isLoaded = m_isCartridgeLoadedFn && m_isCartridgeLoadedFn();

      // 1. Run Without Debugging (Always visible)
      bool canRunNoDebug = (state == DebugState::Disconnected) && !isRunning;
      if (ImGui::MenuItem("Run Without Debugging", "Ctrl+R", false,
                          isLoaded && canRunNoDebug)) {
        if (m_startCartridgeFn)
          m_startCartridgeFn();
        if (m_resumeRuntimeFn)
          m_resumeRuntimeFn();
      }

      // 2. Start Debugging / Continue (Contextual label)
      const char *debugLabel =
          (state == DebugState::Stopped) ? "Continue" : "Start Debugging";
      bool canDebug = isLoaded && (state == DebugState::Disconnected ||
                                   state == DebugState::Stopped);
      // If we are native running, we can't attach debug yet regarding this
      // logic
      if (isRunning && state == DebugState::Disconnected)
        canDebug = false;

      if (ImGui::MenuItem(debugLabel, "F5", false, canDebug)) {
        if (state == DebugState::Stopped) {
          if (m_resumeRuntimeFn)
            m_resumeRuntimeFn();
          m_dapClient.Continue();
        } else {
          m_showDebugger = true;
          m_showBreakpoints = true;
          Document *doc = m_documentSystem.GetActiveDocument();
          if (m_getScriptEngineFn) {
            m_dapClient.SetScriptEngine(m_getScriptEngineFn());
          }

          // Smart Root Detection for Debugger
          // We need m_projectRoot in DapClient to match the Cartridge Root
          std::string debugRoot = m_projectSystem.GetRoot().fullPath;
          if (doc) {
            std::filesystem::path p = doc->path;
            // If debugging main.nut, its folder is the root
            if (p.filename() == "main.nut") {
              debugRoot = p.parent_path().string();
            }
          }
          m_dapClient.SetProjectRoot(debugRoot);

          // 1. Ensure clean state by stopping if running
          // This prevents debug hooks from firing *during* Launch (inside
          // ImGui), which would cause a deadlock/recursive loop.
          if (m_isCartridgeRunningFn && m_isCartridgeRunningFn()) {
            if (m_stopCartridgeFn) {
              fprintf(stderr,
                      "UIShell: Stopping cartridge before debug launch...\n");
              m_stopCartridgeFn();
            }
          }

          // 2. Launch DAP (Syncs BPs, Enable Debug Mode)
          if (doc) {
            m_dapClient.Launch(doc->path);
          } else {
            m_dapClient.Launch("");
          }
          ImGui::SetWindowFocus("Debugger");

          // 3. Start Cartridge
          // This will execute the entry script. If BPs are hit, the engine will
          // block.
          if (m_startCartridgeFn) {
            fprintf(stderr, "UIShell: Calling StartCartridgeFn...\n");
            m_startCartridgeFn();
          } else {
            fprintf(stderr, "UIShell: m_startCartridgeFn is NULL!\n");
          }
        }
      }

      // 3. Pause
      bool canPause = (state == DebugState::Running);
      if (ImGui::MenuItem("Pause", "F6", false, canPause)) {
        m_dapClient.Pause();
      }

      // 4. Stop
      bool canStop = isRunning || (state != DebugState::Disconnected);
      if (ImGui::MenuItem("Stop", "Shift+F5", false, canStop)) {
        // Stop Debugger
        m_dapClient.Stop();
        // Stop/Reload Runtime
        if (m_stopCartridgeFn)
          m_stopCartridgeFn();
      }

      ImGui::Separator();

      bool canStep = (state == DebugState::Stopped);
      if (ImGui::MenuItem("Step Over", "F10", false, canStep)) {
        m_dapClient.StepOver();
      }
      if (ImGui::MenuItem("Step Into", "F11", false, canStep)) {
        m_dapClient.StepIn();
      }
      if (ImGui::MenuItem("Step Out", "Shift+F11", false, canStep)) {
        m_dapClient.StepOut();
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Toggle Breakpoint", "F9")) {
        Document *doc = m_documentSystem.GetActiveDocument();
        if (doc) {
          auto cursors = doc->buffer.GetCursors();
          if (!cursors.empty()) {
            int line = doc->buffer.GetLineFromOffset(cursors[0].head) + 1;
            m_dapClient.ToggleBreakpoint(doc->path, line);
          }
        }
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
  // Guard: Skip nodes with empty names (e.g., uninitialized root)
  if (node.name.empty()) {
    if (!node.children.empty()) {
      // If root has no name but has children, just render children
      for (const auto &child : node.children) {
        DrawTree(child, onOpen);
      }
    }
    return;
  }

  if (node.isDirectory) {
    // Expand directories by default (user can still collapse)
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
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
  // Project Explorer (only shown when a project is open)
  if (m_showExplorer && m_projectSystem.HasProject()) {
    if (ImGui::Begin("Project Explorer")) {
      ImGui::Text("Root: %s", m_projectSystem.GetRoot().name.c_str());
      ImGui::Separator();
      DrawTree(m_projectSystem.GetRoot(), [this](const std::string &path) {
        Document *doc = nullptr;
        m_documentSystem.OpenDocument(path, &doc);
        if (doc)
          m_documentSystem.SetActiveDocument(doc);
      });
    }
    ImGui::End();
  } else if (m_showExplorer) {
    // Show placeholder when no project is open
    if (ImGui::Begin("Project Explorer")) {
      ImGui::TextWrapped("No folder opened.");
      ImGui::TextWrapped("Use File > Open Folder to open a project.");
    }
    ImGui::End();
  }

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
      // Calculate gutter width for line numbers (with space for breakpoint dot)
      char lineNumBuf[16];
      snprintf(lineNumBuf, sizeof(lineNumBuf), "%d", totalLines);
      float lineNumWidth = ImGui::CalcTextSize(lineNumBuf).x;
      float breakpointDotRadius = 5.0f;
      float gutterWidth = breakpointDotRadius * 2 + 8.0f + lineNumWidth + 10.0f;

      // Get breakpoints and debug state for this file
      auto breakpoints = m_dapClient.GetBreakpoints();
      DebugState debugState = m_dapClient.GetState();
      auto callStack = m_dapClient.GetCallStack();
      int currentDebugLine = -1;
      if (debugState == DebugState::Stopped && !callStack.empty() &&
          callStack[0].file == doc->path) {
        currentDebugLine = callStack[0].line; // 1-based
      }

      for (int i = firstLine; i < lastLine; ++i) {
        std::string line = buffer.GetLine(i);
        uint32_t lineStart = buffer.GetLineStart(i); // Offset
        int lineNum = i + 1;                         // 1-based line number

        ImGui::SetCursorPosY((float)i * lineHeight);

        // Check if this line has a breakpoint
        bool hasBreakpoint = false;
        for (const auto &bp : breakpoints) {
          if (bp.file == doc->path && bp.line == lineNum && bp.enabled) {
            hasBreakpoint = true;
            break;
          }
        }

        // Get window draw list for custom rendering
        auto *drawList = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        float scrollX = ImGui::GetScrollX();

        // Calculate gutter rectangle for this line (for click detection)
        ImVec2 gutterMin(winPos.x - scrollX,
                         winPos.y - scrollY + (float)i * lineHeight);
        ImVec2 gutterMax(winPos.x - scrollX + gutterWidth - 5.0f,
                         gutterMin.y + lineHeight);

        // Highlight current debug line (yellow background)
        if (lineNum == currentDebugLine) {
          ImVec2 rowMin(winPos.x - scrollX,
                        winPos.y - scrollY + (float)i * lineHeight);
          ImVec2 rowMax(winPos.x - scrollX + ImGui::GetWindowWidth(),
                        rowMin.y + lineHeight);
          drawList->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 0, 40));
          // Yellow arrow indicator
          float arrowX = winPos.x - scrollX + breakpointDotRadius + 2;
          float arrowY =
              winPos.y - scrollY + (float)i * lineHeight + lineHeight / 2;
          drawList->AddTriangleFilled(
              ImVec2(arrowX - 4, arrowY - 4), ImVec2(arrowX + 4, arrowY),
              ImVec2(arrowX - 4, arrowY + 4), IM_COL32(255, 200, 0, 255));
        }

        // Draw breakpoint red dot
        if (hasBreakpoint) {
          float dotX = winPos.x - scrollX + breakpointDotRadius + 2;
          float dotY =
              winPos.y - scrollY + (float)i * lineHeight + lineHeight / 2;
          drawList->AddCircleFilled(ImVec2(dotX, dotY), breakpointDotRadius,
                                    IM_COL32(255, 60, 60, 255));
        }

        // Invisible button for gutter click to toggle breakpoint
        ImGui::SetCursorPosX(0);
        ImGui::SetCursorPosY((float)i * lineHeight);
        std::string gutterBtnId = "##gutter" + std::to_string(i);
        if (ImGui::InvisibleButton(
                gutterBtnId.c_str(),
                ImVec2(gutterWidth - lineNumWidth - 5, lineHeight))) {
          fprintf(stderr, "UIShell: Gutter clicked line %d\n", lineNum);
          m_dapClient.ToggleBreakpoint(doc->path, lineNum);
        }

        // Render line number
        ImGui::SameLine();
        ImGui::SetCursorPosX(gutterWidth - lineNumWidth - 5);
        snprintf(lineNumBuf, sizeof(lineNumBuf), "%d", lineNum);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(128, 128, 128, 255));
        ImGui::TextUnformatted(lineNumBuf);
        ImGui::PopStyleColor();

        // Move to text area (after gutter)
        ImGui::SameLine();
        ImGui::SetCursorPosX(gutterWidth);

        // Highlighted render
        if (parseRes && !parseRes->highlights.empty()) {
          float x = gutterWidth;
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
            ImVec2(winPos.x - scrollX + gutterWidth + headX,
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

            ImVec2 pMin(winPos.x - scrollX + gutterWidth + x1,
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
    // Run/Stop buttons
    if (!m_previewRunning) {
      if (ImGui::Button("Run Preview")) {
        if (m_loadCartridgeFn && m_projectSystem.HasProject()) {
          std::string projectPath = m_projectSystem.GetRoot().fullPath;
          if (m_loadCartridgeFn(projectPath)) {
            m_previewRunning = true;
          }
        }
      }
    } else {
      if (ImGui::Button("Stop Preview")) {
        m_previewRunning = false;
        // Note: Runtime continues, we just hide the preview
      }
    }

    ImGui::Separator();

    // Preview area
    ImGui::BeginChild("PreviewArea", ImVec2(0, 0), true);

    if (m_previewRunning && m_getPreviewTextureFn && m_getPreviewSizeFn) {
      void *texPtr = m_getPreviewTextureFn();
      if (texPtr) {
        uint32_t texW = 0, texH = 0;
        m_getPreviewSizeFn(texW, texH);

        if (texW > 0 && texH > 0) {
          // Calculate size maintaining aspect ratio
          ImVec2 availSize = ImGui::GetContentRegionAvail();
          float texAspect = (float)texW / (float)texH;
          float availAspect = availSize.x / availSize.y;

          ImVec2 imageSize;
          if (texAspect > availAspect) {
            imageSize.x = availSize.x;
            imageSize.y = availSize.x / texAspect;
          } else {
            imageSize.y = availSize.y;
            imageSize.x = availSize.y * texAspect;
          }

          // Center the image
          ImVec2 cursorPos = ImGui::GetCursorPos();
          ImGui::SetCursorPos(
              ImVec2(cursorPos.x + (availSize.x - imageSize.x) * 0.5f,
                     cursorPos.y + (availSize.y - imageSize.y) * 0.5f));

          ImGui::Image(texPtr, imageSize);

          // Status bar
          ImGui::SetCursorPos(
              ImVec2(cursorPos.x, cursorPos.y + availSize.y - 20));
          ImGui::Text("Resolution: %ux%u", texW, texH);
        } else {
          ImGui::Text("Starting...");
        }
      } else {
        ImGui::Text("Waiting for frame...");
      }
    } else if (!m_projectSystem.HasProject()) {
      ImGui::TextWrapped("No project open.");
      ImGui::TextWrapped("Use File > Open Folder to open a project.");
    } else {
      ImGui::Text("Click 'Run Preview' to start.");
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

  // Timeline Pane (visibility controlled)
  if (m_showLocalHistory) {
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
  }

  // Debugger Control Bar (visibility controlled)
  if (m_showDebugger) {
    if (ImGui::Begin("Debugger")) {
      DebugState state = m_dapClient.GetState();

      // Status indicator
      const char *stateText = "Disconnected";
      ImVec4 stateColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
      if (state == DebugState::Running) {
        stateText = "Running";
        stateColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
      } else if (state == DebugState::Stopped) {
        stateText = "Paused";
        stateColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
      } else if (state == DebugState::Terminated) {
        stateText = "Terminated";
        stateColor = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
      }
      ImGui::TextColored(stateColor, "Status: %s", stateText);

      if (state == DebugState::Disconnected) {
        Document *doc = m_documentSystem.GetActiveDocument();
        if (doc && ImGui::Button("Start Debugging (F5)")) {
          m_dapClient.Launch(doc->path);
        }
      } else if (state == DebugState::Stopped) {
        if (ImGui::Button("Continue (F5)"))
          m_dapClient.Continue();
        ImGui::SameLine();
        if (ImGui::Button("Step Over (F10)"))
          m_dapClient.StepOver();
        ImGui::SameLine();
        if (ImGui::Button("Step Into (F11)"))
          m_dapClient.StepIn();
        ImGui::SameLine();
        if (ImGui::Button("Step Out"))
          m_dapClient.StepOut();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
          m_dapClient.Stop();
      } else if (state == DebugState::Running) {
        if (ImGui::Button("Pause (F6)"))
          m_dapClient.Pause();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
          m_dapClient.Stop();
      } else {
        if (ImGui::Button("Restart"))
          m_dapClient.Disconnect();
      }

      ImGui::Separator();

      // Call Stack
      auto stack = m_dapClient.GetCallStack();
      if (ImGui::CollapsingHeader("Call Stack",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        if (stack.empty()) {
          ImGui::TextDisabled("(no call stack)");
        } else {
          for (size_t i = 0; i < stack.size(); ++i) {
            const auto &frame = stack[i];
            std::string filename =
                std::filesystem::path(frame.file).filename().string();
            std::string label = frame.name + " @ " + filename + ":" +
                                std::to_string(frame.line);

            // Highlight current frame
            if (i == 0) {
              ImGui::PushStyleColor(ImGuiCol_Text,
                                    ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            }

            if (ImGui::Selectable(label.c_str())) {
              // Navigate to frame location
              Document *doc = nullptr;
              if (m_documentSystem.OpenDocument(frame.file, &doc).ok()) {
                m_documentSystem.SetActiveDocument(doc);
                doc->buffer.SetCursor(doc->buffer.GetLineStart(frame.line - 1));
              }
            }

            if (i == 0) {
              ImGui::PopStyleColor();
            }
          }
        }
      }

      // Variables / Locals
      if (ImGui::CollapsingHeader("Locals", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto locals = m_dapClient.GetLocals(0);
        if (locals.empty()) {
          ImGui::TextDisabled("(no local variables)");
        } else {
          for (const auto &var : locals) {
            ImGui::Text("%s", var.name.c_str());
            ImGui::SameLine(100);
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s",
                               var.value.c_str());
          }
        }
      }
    }
    ImGui::End();
  }

  // Breakpoints pane (visibility controlled)
  if (m_showBreakpoints) {
    if (ImGui::Begin("Breakpoints")) {
      auto breakpoints = m_dapClient.GetBreakpoints();
      for (const auto &bp : breakpoints) {
        std::string label = std::filesystem::path(bp.file).filename().string() +
                            ":" + std::to_string(bp.line);
        bool enabled = bp.enabled;
        if (ImGui::Checkbox(("##bp" + std::to_string(bp.id)).c_str(),
                            &enabled)) {
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
  }

  // Problems pane (visibility controlled)
  if (m_showProblems) {
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

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
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

void UIShell::RenderOutputPane() {
  if (ImGui::Begin("Output")) {
    ImGui::TextWrapped("Build output and logs will appear here.");
    // TODO: Connect to build system output
  }
  ImGui::End();
}

void UIShell::RenderConsolePane() {
  if (ImGui::Begin("Console")) {
    ImGui::TextWrapped("Debug console - script output and REPL.");
    // TODO: Connect to script engine console
  }
  ImGui::End();
}

void UIShell::RenderPreviewPane() {
  if (ImGui::Begin("Preview")) {
    ImGui::TextWrapped("Game preview will appear here.");
    // TODO: Render game viewport texture
    ImGui::Text("Resolution: 256x240");
    ImGui::Text("FPS: 60");
  }
  ImGui::End();
}

void UIShell::RenderFolderDialog() {
  ImGui::OpenPopup("Open Folder");

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 400));

  if (ImGui::BeginPopupModal("Open Folder", &m_showFolderDialog,
                             ImGuiWindowFlags_NoResize)) {
    ImGui::Text("Current Path:");
    ImGui::TextWrapped("%s", m_folderDialogPath.c_str());
    ImGui::Separator();

    // Parent directory button
    if (ImGui::Button("..")) {
      std::filesystem::path p(m_folderDialogPath);
      if (p.has_parent_path()) {
        m_folderDialogPath = p.parent_path().string();
      }
    }
    ImGui::SameLine();
    ImGui::Text("(Parent Folder)");

    ImGui::Separator();

    // List directories
    ImGui::BeginChild("DirectoryList", ImVec2(0, -50), true);
    try {
      for (const auto &entry :
           std::filesystem::directory_iterator(m_folderDialogPath)) {
        if (entry.is_directory()) {
          std::string name = entry.path().filename().string();
          // Skip hidden directories
          if (!name.empty() && name[0] != '.') {
            // Check if folder has main.nut (valid project)
            std::filesystem::path mainNutPath = entry.path() / "main.nut";
            bool isProject = std::filesystem::exists(mainNutPath);

            // Add icon prefix for project folders
            std::string label = isProject ? "[P] " + name : name;

            if (ImGui::Selectable(label.c_str(), false,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
              if (ImGui::IsMouseDoubleClicked(0)) {
                if (isProject) {
                  // Double-click on project: open it directly
                  m_folderDialogPath = entry.path().string();

                  // Stop any running preview and clear canvas
                  m_previewRunning = false;
                  if (m_clearPreviewFn) {
                    m_clearPreviewFn();
                  }

                  // Open the project
                  m_projectSystem.OpenRoot(m_folderDialogPath);

                  // Auto-open main.nut
                  Document *doc = nullptr;
                  if (m_documentSystem.OpenDocument(mainNutPath.string(), &doc)
                          .ok()) {
                    m_documentSystem.SetActiveDocument(doc);
                  }

                  // Auto-load and start preview
                  if (m_loadCartridgeFn &&
                      m_loadCartridgeFn(m_folderDialogPath)) {
                    m_previewRunning = true;
                  }

                  m_showPreview = true;
                  m_folderDialogError.clear();
                  m_showFolderDialog = false;
                  ImGui::CloseCurrentPopup();
                } else {
                  // Double-click on regular folder: navigate into it
                  m_folderDialogPath = entry.path().string();
                }
              }
            }
          }
        }
      }
    } catch (...) {
      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Cannot read directory");
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Show error if folder doesn't have main.nut
    if (!m_folderDialogError.empty()) {
      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s",
                         m_folderDialogError.c_str());
    }

    // Action buttons
    if (ImGui::Button("Open", ImVec2(100, 0))) {
      // Validate: folder must contain main.nut
      std::filesystem::path mainNut =
          std::filesystem::path(m_folderDialogPath) / "main.nut";
      if (!std::filesystem::exists(mainNut)) {
        m_folderDialogError = "Invalid project: main.nut not found";
      } else {
        // Stop any running preview and clear canvas
        m_previewRunning = false;
        if (m_clearPreviewFn) {
          m_clearPreviewFn();
        }

        m_projectSystem.OpenRoot(m_folderDialogPath);

        // Auto-open main.nut
        Document *doc = nullptr;
        if (m_documentSystem.OpenDocument(mainNut.string(), &doc).ok()) {
          m_documentSystem.SetActiveDocument(doc);
        }

        // Auto-load and start preview
        if (m_loadCartridgeFn && m_loadCartridgeFn(m_folderDialogPath)) {
          m_previewRunning = true;
        }

        m_showPreview = true;
        m_folderDialogError.clear();
        m_showFolderDialog = false;
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      m_folderDialogError.clear();
      m_showFolderDialog = false;
      ImGui::CloseCurrentPopup();
    }

    // ESC to close
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      m_folderDialogError.clear();
      m_showFolderDialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void UIShell::RenderNewProjectDialog() {
  ImGui::OpenPopup("New Project");

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(400, 180));

  if (ImGui::BeginPopupModal("New Project", &m_showNewProjectDialog,
                             ImGuiWindowFlags_NoResize)) {
    ImGui::Text("Project Name:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ProjectName", m_newProjectName,
                     sizeof(m_newProjectName));

    // Show error if any
    if (!m_newProjectError.empty()) {
      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s",
                         m_newProjectError.c_str());
    }

    ImGui::Separator();

    if (ImGui::Button("Create", ImVec2(100, 0))) {
      std::string name(m_newProjectName);
      if (name.empty()) {
        m_newProjectError = "Project name cannot be empty";
      } else {
        // Create folder in samples/
        std::filesystem::path samplesPath =
            std::filesystem::current_path() / "samples";
        std::filesystem::path projectPath = samplesPath / name;

        if (std::filesystem::exists(projectPath)) {
          m_newProjectError = "Project already exists";
        } else {
          try {
            // Create directory
            std::filesystem::create_directories(projectPath);

            // Create empty main.nut
            std::filesystem::path mainNut = projectPath / "main.nut";
            std::ofstream ofs(mainNut);
            ofs << "// " << name << " - main.nut\n";
            ofs << "// Created by ARCANEE IDE\n";
            ofs.close();

            // Open the new project
            m_projectSystem.OpenRoot(projectPath.string());

            // Open main.nut in editor
            Document *doc = nullptr;
            if (m_documentSystem.OpenDocument(mainNut.string(), &doc).ok()) {
              m_documentSystem.SetActiveDocument(doc);
            }

            m_showPreview = true;
            m_newProjectError.clear();
            m_showNewProjectDialog = false;
            ImGui::CloseCurrentPopup();
          } catch (const std::exception &e) {
            m_newProjectError =
                std::string("Failed to create project: ") + e.what();
          }
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      m_newProjectError.clear();
      m_showNewProjectDialog = false;
      ImGui::CloseCurrentPopup();
    }

    // ESC to close
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      m_newProjectError.clear();
      m_showNewProjectDialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

} // namespace arcanee::ide
