#pragma once

#include "BreakpointStore.h"
#include <functional>
#include <squirrel.h>
#include <string>

namespace arcanee::script {

class ScriptEngine;

// Debug action for stepping
enum class DebugAction { None, StepIn, StepOver, StepOut, Continue, Pause };

class ScriptDebugger {
public:
  explicit ScriptDebugger(ScriptEngine *engine);
  ~ScriptDebugger();

  // Lifecycle
  void attach(HSQUIRRELVM vm);
  void detach();

  // API
  void setEnabled(bool enabled);
  bool isEnabled() const { return m_enabled; }

  void setPaused(bool paused);
  bool isPaused() const { return m_paused; }

  // Actions
  void setAction(DebugAction action);
  void resume(); // Helper for Continue

  // Breakpoints
  BreakpointStore &getBreakpoints() { return m_breakpoints; }

  // Hook
  static void debugHook(HSQUIRRELVM v, SQInteger type, const SQChar *sourcename,
                        SQInteger line, const SQChar *funcname);

  // Callbacks
  using StopCallback = std::function<void(int line, const std::string &file,
                                          const std::string &reason)>;
  void setStopCallback(StopCallback cb) { m_onStop = cb; }

  // UI pump callback - called while waiting at breakpoint to keep UI responsive
  using UIPumpCallback = std::function<void()>;
  void setUIPumpCallback(UIPumpCallback cb) { m_uiPump = cb; }

  // Should exit callback - returns true when app wants to quit
  using ShouldExitCallback = std::function<bool()>;
  void setShouldExitCallback(ShouldExitCallback cb) { m_shouldExit = cb; }

private:
  void onHook(HSQUIRRELVM v, SQInteger type, const std::string &file, int line,
              const std::string &func);

  ScriptEngine *m_engine;
  HSQUIRRELVM m_vm = nullptr;
  BreakpointStore m_breakpoints;

  bool m_enabled = false;
  bool m_paused = false;

  // Step State
  DebugAction m_action = DebugAction::None;
  int m_stepDepth = 0;    // Depth when step started
  int m_currentDepth = 0; // Current call stack depth (tracked)

  StopCallback m_onStop;
  UIPumpCallback m_uiPump;
  ShouldExitCallback m_shouldExit;
};

} // namespace arcanee::script
