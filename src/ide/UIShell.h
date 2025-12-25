#pragma once
#include "common/Status.h"
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string_view>
#include <vector>

#include "DapClient.h"
#include "DocumentSystem.h"
#include "LspClient.h"
#include "ParseService.h"
#include "ProjectSystem.h"
#include "SearchService.h"
#include "TaskRunner.h"
#include "TimelineStore.h"

namespace arcanee::script {
class ScriptEngine; // Forward declaration
}

// Forward declaration of ImGui structures if needed, but we typically use
// internal dispatch.

namespace arcanee::ide {

struct CommandId {
  uint32_t v{};
};

struct CommandContext {
  // Active doc, selections, etc.
  // For now simple placeholder
};

using CommandFn = std::function<Status(const CommandContext &)>;

class MainThreadQueue {
public:
  using Job = std::function<void()>;

  // Thread-safe push
  void Push(Job job);

  // UI-thread only
  void DrainAll();

private:
  std::mutex m_mutex;
  std::vector<Job> m_jobs;
};

class UIShell {
public:
  explicit UIShell(MainThreadQueue &queue);
  ~UIShell();

  // Register a command for the palette
  Status RegisterCommand(std::string_view name, CommandFn fn, CommandId *out);

  // Render the entire IDE frame (Dockspace + Panes)
  void RenderFrame();

  // Accessor
  MainThreadQueue &Queue() { return m_queue; }

private:
  void RenderDockspace();
  void RenderPanes();
  void RenderSearchPane();
  void RenderCommandPalette();
  void RenderOutputPane();
  void RenderConsolePane();
  void RenderPreviewPane();
  void RenderFolderDialog();
  void RenderNewProjectDialog();

  MainThreadQueue &m_queue;

  // State
  bool m_showCommandPalette = false;
  bool m_showFolderDialog = false;
  std::string m_folderDialogPath;
  std::string m_folderDialogError; // Error message for folder validation
  bool m_showNewProjectDialog = false;
  char m_newProjectName[256] = {};
  std::string m_newProjectError;
  std::function<void()> m_requestExitFn;

  // Preview callbacks
  std::function<bool(const std::string &)> m_loadCartridgeFn;
  std::function<void *()> m_getPreviewTextureFn;
  std::function<void(uint32_t &, uint32_t &)> m_getPreviewSizeFn;
  std::function<void()> m_clearPreviewFn;
  std::function<void()> m_pauseRuntimeFn;
  std::function<void()> m_resumeRuntimeFn;
  std::function<script::ScriptEngine *()> m_getScriptEngineFn;
  bool m_previewRunning = false;

public:
  void SetRequestExitFn(std::function<void()> fn) {
    m_requestExitFn = std::move(fn);
  }

  void SetLoadCartridgeFn(std::function<bool(const std::string &)> fn) {
    m_loadCartridgeFn = std::move(fn);
  }

  void SetGetPreviewTextureFn(std::function<void *()> fn) {
    m_getPreviewTextureFn = std::move(fn);
  }

  void SetGetPreviewSizeFn(std::function<void(uint32_t &, uint32_t &)> fn) {
    m_getPreviewSizeFn = std::move(fn);
  }

  void SetClearPreviewFn(std::function<void()> fn) {
    m_clearPreviewFn = std::move(fn);
  }

  void SetPauseRuntimeFn(std::function<void()> fn) {
    m_pauseRuntimeFn = std::move(fn);
  }

  void SetResumeRuntimeFn(std::function<void()> fn) {
    m_resumeRuntimeFn = std::move(fn);
  }

  void SetGetScriptEngineFn(std::function<script::ScriptEngine *()> fn) {
    m_getScriptEngineFn = std::move(fn);
  }

private:
  // Persistence
  void LoadLayout();
  void SaveLayout();

  // Core Services
  ProjectSystem m_projectSystem;
  DocumentSystem m_documentSystem;
  ParseService m_parseService;
  SearchService m_searchService;

  // Search UI State
  std::string m_searchQuery;
  bool m_searchCaseSensitive = false;
  bool m_searchRegex = false;

  // Pane Visibility (Code View defaults - hide debug panes)
  bool m_showExplorer = true;
  bool m_showEditor = true;  // Always visible
  bool m_showSearch = false; // Toggle with Ctrl+Shift+F
  bool m_showOutput = true;
  bool m_showConsole = false;
  bool m_showProblems = true;
  bool m_showDebugger = true;    // Docked in bottom panel
  bool m_showBreakpoints = true; // Docked in left sidebar
  bool m_showLocalHistory = false;
  bool m_showPreview = true; // Preview pane on right

  TaskRunner m_taskRunner;
  int m_selectedTaskIndex = -1;

  TimelineStore m_timelineStore;
  DapClient m_dapClient;
  LspClient m_lspClient;
};

} // namespace arcanee::ide
