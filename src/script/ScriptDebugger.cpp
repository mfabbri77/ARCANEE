#include "ScriptDebugger.h"
#include "ScriptEngine.h"
#include "common/Log.h"
#include "platform/Time.h"
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
  if (action != DebugAction::None && action != DebugAction::Continue) {
    // Record depth for stepping
    m_stepDepth = m_currentDepth;
    // Arm step - next line event will capture start location
    m_stepArmed = true;
    LOG_INFO("Debugger: Action=%d, StartDepth=%d, Armed", (int)action,
             m_stepDepth);
  } else {
    m_stepArmed = false;
  }
}

void ScriptDebugger::resume() {
  if (m_paused && m_vm) {
    m_paused = false;
    // Wake up the VM
    // We pass 1 because we want to return a value?
    // Actually when suspending we usually just return.
    // sq_wakeupvm(v, wakeupRet, raiseError, sourceIsRet)
    // Wake up the VM
    sq_wakeupvm(m_vm, SQFalse, SQFalse, SQTrue, SQFalse);
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
    LOG_INFO("CALL depth++ -> %d", m_currentDepth);
    return;
  }
  if (type == 'r') {
    m_currentDepth--;
    LOG_INFO("RETURN depth-- -> %d", m_currentDepth);
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

  // BP CHECK LOG
  fprintf(stderr,
          "BP CHECK file='%s' line=%d enabled=%d action=%d paused=%d armed=%d "
          "hit=%d\n",
          file.c_str(), line, (int)m_enabled, (int)m_action, (int)m_paused,
          (int)m_stepArmed, (int)m_breakpoints.hasBreakpoint(file, line));
  fflush(stderr);

  // Breakpoints are checked EVEN WHEN action == Continue
  if (m_breakpoints.hasBreakpoint(file, line)) {
    LOG_INFO("Hit breakpoint at %s:%d", file.c_str(), line);
    m_paused = true;
    m_action = DebugAction::None;
    m_stepArmed = false;
    if (m_onStop)
      m_onStop(line, file, "breakpoint");

    // BLOCKING WAIT: Spin here until debugger is resumed or app should exit
    LOG_INFO("Debugger paused at %s:%d - waiting for continue", file.c_str(),
             line);
    while (m_paused) {
      if (m_shouldExit && m_shouldExit()) {
        LOG_INFO("Exit requested - breaking debug pause");
        m_paused = false;
        break;
      }
      if (m_uiPump) {
        m_uiPump();
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    LOG_INFO("Debugger resumed from %s:%d", file.c_str(), line);
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
    LOG_INFO("Step armed at %s:%d depth=%d", file.c_str(), line,
             m_currentDepth);
    return; // Skip first event at start location
  }

  // Stepping checks - only stop if location changed
  bool shouldStop = false;
  bool locationChanged = (file != m_stepStartFile || line != m_stepStartLine);

  switch (m_action) {
  case DebugAction::StepIn:
    // Stop on the first line where location changed
    shouldStop = locationChanged;
    break;
  case DebugAction::StepOver:
    // Stop when location changed AND we're at same or shallower depth
    shouldStop = locationChanged && (m_currentDepth <= m_stepDepth);
    break;
  case DebugAction::StepOut:
    // Stop when we've returned to a shallower depth
    shouldStop = (m_currentDepth < m_stepDepth);
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

    // BLOCKING WAIT
    LOG_INFO("Debugger paused at %s:%d - waiting for continue", file.c_str(),
             line);
    while (m_paused) {
      if (m_shouldExit && m_shouldExit()) {
        LOG_INFO("Exit requested - breaking debug pause");
        m_paused = false;
        break;
      }
      if (m_uiPump) {
        m_uiPump();
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    LOG_INFO("Debugger resumed from %s:%d", file.c_str(), line);
  }
}

} // namespace arcanee::script
