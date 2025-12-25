#include "ScriptDebugger.h"
#include "ScriptEngine.h"
#include "common/Log.h"
#include "platform/Time.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sqstdaux.h>
#include <thread>

namespace arcanee::script {

ScriptDebugger::ScriptDebugger(ScriptEngine *engine) : m_engine(engine) {}

ScriptDebugger::~ScriptDebugger() { detach(); }

void ScriptDebugger::attach(HSQUIRRELVM vm) {
  m_vm = vm;
  if (m_enabled) {
    sq_setnativedebughook(m_vm, debugHook);
  }
}

void ScriptDebugger::detach() {
  if (m_vm) {
    sq_setnativedebughook(m_vm, nullptr);
    m_vm = nullptr;
  }
}

void ScriptDebugger::setEnabled(bool enabled) {
  m_enabled = enabled;
  if (m_vm) {
    if (enabled) {
      sq_setnativedebughook(m_vm, debugHook);
    } else {
      sq_setnativedebughook(m_vm, nullptr);
    }
  }
}

void ScriptDebugger::setPaused(bool paused) { m_paused = paused; }

void ScriptDebugger::setAction(DebugAction action) {
  m_action = action;
  if (action != DebugAction::None && action != DebugAction::Continue &&
      action != DebugAction::Pause) {
    // Record depth for stepping
    m_stepDepth = m_currentDepth;
    // Arm step - next line event will capture start location
    m_stepArmed = true;
  } else {
    m_stepArmed = false;
  }
}

void ScriptDebugger::debugHook(HSQUIRRELVM v, SQInteger type,
                               const SQChar *sourcename, SQInteger line,
                               const SQChar *funcname) {
  ScriptEngine *engine = (ScriptEngine *)sq_getforeignptr(v);

  // Diagnostic log for instance identity
  if (type == 'l') {
    LOG_INFO("debugHook engine=%p vm=%p debugger=%p file=%s line=%d type=%c",
             engine, v, engine ? engine->getDebugger() : nullptr,
             sourcename ? sourcename : "", (int)line, (char)type);
  }

  if (engine && engine->getDebugger()) {
    std::string file = sourcename ? sourcename : "";
    std::string func = funcname ? funcname : "";
    LOG_INFO("PRE-ONHOOK: About to call onHook for %s:%d", file.c_str(),
             (int)line);
    engine->getDebugger()->onHook(v, type, file, (int)line, func);
    LOG_INFO("POST-ONHOOK: Returned from onHook for %s:%d", file.c_str(),
             (int)line);
  }
}

void ScriptDebugger::onHook(HSQUIRRELVM v, SQInteger type,
                            const std::string &file, int line,
                            const std::string &func) {
  // UNCONDITIONAL entry probe
  LOG_INFO("ONHOOK ENTRY type=%c file=%s line=%d depth=%d", (char)type,
           file.c_str(), line, m_currentDepth);

  // Track call depth on 'c' (call) and 'r' (return) events FIRST
  // This must happen BEFORE any early returns so we maintain accurate depth
  if (type == 'c') {
    m_currentDepth++;
    return;
  }
  if (type == 'r') {
    m_currentDepth =
        std::max(0, m_currentDepth - 1); // Clamp to prevent negative
    return;
  }

  // Only line events ('l') are relevant for line breakpoints and stepping
  if (type != 'l')
    return;

  // Watchdog Check - SKIP if debugging is enabled!
  if (m_engine && m_engine->m_watchdogEnabled && !m_enabled) {
    double elapsed = platform::Time::now() - m_engine->m_executionStartTime;
    if (elapsed > m_engine->m_watchdogTimeout) {
      fprintf(stderr, "WATCHDOG TIMEOUT at %s:%d\n", file.c_str(), line);
      sq_throwerror(v, "Watchdog timeout: Execution time limit exceeded");
      return;
    }
  }

  // Pause action: stop on next line event
  if (m_action == DebugAction::Pause) {
    m_paused = true;
    m_action = DebugAction::None;
    if (m_onStop)
      m_onStop(line, file, "pause");
    // Block here until resumed - sq_suspendvm doesn't work from debug hook
    while (m_paused && !(m_shouldExit && m_shouldExit())) {
      if (m_uiPump)
        m_uiPump();
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    m_paused = false; // Ensure paused is cleared on exit
    return;
  }

  // Breakpoints are checked EVEN WHEN action == Continue
  // Breakpoint check
  if (m_breakpoints.hasBreakpoint(file, line)) {
    LOG_INFO("Hit breakpoint at %s:%d", file.c_str(), line);
    m_paused = true;
    m_action = DebugAction::None;
    m_stepArmed = false;
    if (m_onStop)
      m_onStop(line, file, "breakpoint");
    // Block here until resumed - sq_suspendvm doesn't work from debug hook
    while (m_paused && !(m_shouldExit && m_shouldExit())) {
      if (m_uiPump)
        m_uiPump();
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    m_paused = false; // Ensure paused is cleared on exit
    return;
  }

  // If just running (Continue), no stepping logic needed
  if (m_action == DebugAction::Continue || m_action == DebugAction::None) {
    return;
  }

  // STEP ARM: Capture start location on first line event after step initiated
  if (m_stepArmed) {
    m_stepStartFile = file;
    m_stepStartLine = line;
    m_stepArmed = false;
    m_stepEventCount = 0; // Reset step counter
    return;               // Skip first event at start location
  }
  m_stepEventCount++; // Count line events for same-line detection

  // Stepping checks - only stop if location changed
  bool shouldStop = false;
  bool locationChanged = (file != m_stepStartFile || line != m_stepStartLine);

  switch (m_action) {
  case DebugAction::StepIn:
    // Stop on location change OR after at least one line event (same-line case)
    shouldStop = locationChanged || m_stepEventCount > 0;
    break;
  case DebugAction::StepOver:
    // Stop when location changed AND we're at same or shallower depth
    shouldStop = locationChanged && (m_currentDepth <= m_stepDepth);
    break;
  case DebugAction::StepOut:
    // Stop when depth decreased AND location changed (avoid stopping in-place)
    shouldStop = (m_currentDepth < m_stepDepth) && locationChanged;
    break;
  default:
    break;
  }

  if (shouldStop) {
    LOG_INFO("Step stop at %s:%d (action=%d, depth=%d, startDepth=%d)",
             file.c_str(), line, (int)m_action, m_currentDepth, m_stepDepth);
    m_paused = true;
    m_action = DebugAction::None;
    if (m_onStop)
      m_onStop(line, file, "step");
    // Block here until resumed - sq_suspendvm doesn't work from debug hook
    while (m_paused && !(m_shouldExit && m_shouldExit())) {
      if (m_uiPump)
        m_uiPump();
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    m_paused = false; // Ensure paused is cleared on exit
  }
}

} // namespace arcanee::script
