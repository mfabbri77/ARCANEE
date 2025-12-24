#pragma once
#include "common/Status.h"
#include <functional>
#include <map>
#include <mutex>
#include <string_view>
#include <vector>

#include "DocumentSystem.h"
#include "ParseService.h"
#include "ProjectSystem.h"
#include "SearchService.h"
#include "TaskRunner.h"

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

  MainThreadQueue &m_queue;

  // State
  bool m_showCommandPalette = false;

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

  TaskRunner m_taskRunner;
  int m_selectedTaskIndex = -1;
};

} // namespace arcanee::ide
