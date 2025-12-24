#pragma once

#include "common/Log.h"
#include "platform/Window.h"
#include "render/RenderDevice.h"
#include <SDL2/SDL_events.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef ARCANEE_ENABLE_IDE
#include "ide/UIShell.h"
#endif

namespace arcanee::app {

class Runtime;

class Workbench {
public:
  Workbench();
  ~Workbench();

  // Prevent copying
  Workbench(const Workbench &) = delete;
  Workbench &operator=(const Workbench &) = delete;

  bool initialize(render::RenderDevice *device, platform::Window *window,
                  Runtime *runtime);
  void shutdown();

  void update(double dt);
  void render(render::RenderDevice *device);
  bool handleInput(const SDL_Event *event);

  void toggle() { m_visible = !m_visible; }
  bool isVisible() const { return m_visible; }

private:
  bool m_visible = true;
  bool m_initialized = false;
  Runtime *m_runtime = nullptr;

  struct Impl;
  std::unique_ptr<Impl> m_impl;

#ifdef ARCANEE_ENABLE_IDE
  std::unique_ptr<ide::MainThreadQueue> m_mainQueue;
  std::unique_ptr<ide::UIShell> m_uiShell;
#else
  // Legacy MVP Workbench State (Keep explicitly until removed)

  // Project Browser State
  std::string m_projectsDir = "samples";
  std::vector<std::string> m_projectList;
  bool m_showProjectBrowser = true;

  void scanProjects();
  void drawProjectBrowser();

  // Code Editor State
  bool m_showCodeEditor = false;
  std::string m_currentFilePath;
  std::string m_codeEditorContent;
  void openFile(const std::string &path);
  void saveFile();
  void drawCodeEditor();

  // Log Console State
  struct ConsoleLogEntry {
    arcanee::LogLevel level;
    std::string text;
  };
  std::vector<ConsoleLogEntry> m_logs;
  std::mutex m_logMutex;
  bool m_showLogConsole = true;
  bool m_autoScrollLog = true;
  size_t m_logCallbackHandle = 0;
  void drawLogConsole();
#endif
};

} // namespace arcanee::app
