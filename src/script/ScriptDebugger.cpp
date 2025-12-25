#include "ScriptDebugger.h"
#include "ScriptEngine.h"
#include "common/Log.h"
#include "platform/Time.h"
#include <sqstdaux.h>

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
    LOG_INFO("Debugger: Action=%d, StartDepth=%d", (int)action, m_stepDepth);
  }
}

void ScriptDebugger::resume() {
  if (m_paused && m_vm) {
    m_paused = false;
    // Wake up the VM
    // We pass 1 because we want to return a value?
    // Actually when suspending we usually just return.
    // sq_wakeupvm(v, wakeupRet, raiseError, sourceIsRet)
    sq_wakeupvm(m_vm, SQFalse, SQFalse, SQTrue, SQFalse);
  }
}

void ScriptDebugger::debugHook(HSQUIRRELVM v, SQInteger type,
                               const SQChar *sourcename, SQInteger line,
                               const SQChar *funcname) {
  ScriptEngine *engine = (ScriptEngine *)sq_getforeignptr(v);
  if (engine && engine->getDebugger()) {
    std::string file = sourcename ? sourcename : "";
    std::string func = funcname ? funcname : "";
    engine->getDebugger()->onHook(v, type, file, (int)line, func);
  }
}

void ScriptDebugger::onHook(HSQUIRRELVM v, SQInteger type,
                            const std::string &file, int line,
                            const std::string &func) {
  // Watchdog Check
  if (m_engine && m_engine->m_watchdogEnabled) {
    // Only check occasionally or on every line?
    // Every line is fine for now, or use a counter.
    double elapsed = platform::Time::now() - m_engine->m_executionStartTime;
    if (elapsed > m_engine->m_watchdogTimeout) {
      // Log only once?
      sq_throwerror(v, "Watchdog timeout: Execution time limit exceeded");
      return;
    }
  }

  // Track Depth
  if (type == 'c') {
    m_currentDepth++;
  } else if (type == 'r') {
    m_currentDepth--;
  }

  // Only check breakpoints/stepping on line events
  if (type != 'l')
    return;

  bool shouldStop = false;
  std::string reason;

  // 1. Check Breakpoints
  if (m_breakpoints.hasBreakpoint(file, line)) {
    shouldStop = true;
    reason = "breakpoint";
  }

  // 2. Check Stepping
  if (!shouldStop && m_action != DebugAction::None) {
    switch (m_action) {
    case DebugAction::StepIn:
      shouldStop = true;
      reason = "step";
      break;
    case DebugAction::StepOver:
      if (m_currentDepth <= m_stepDepth) {
        shouldStop = true;
        reason = "step";
      }
      break;
    case DebugAction::StepOut:
      // Stop when we return to depth < start depth
      if (m_currentDepth < m_stepDepth) {
        shouldStop = true;
        reason = "step";
      }
      break;
    default:
      break;
    }
  }

  if (shouldStop) {
    m_paused = true;
    m_action = DebugAction::None; // Clear action

    LOG_INFO("Debugger: Paused at %s:%d (%s). Depth=%d", file.c_str(), line,
             reason.c_str(), m_currentDepth);

    if (m_onStop) {
      m_onStop(line, file, reason);
    }

    // SUSPEND EXECUTION
    // This returns control to the caller of sq_call/sq_resume
    sq_suspendvm(v);
  }
}

} // namespace arcanee::script
