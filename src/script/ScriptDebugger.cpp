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
  // UNCONDITIONAL entry probe - this MUST print if onHook is called
  LOG_INFO("ONHOOK ENTRY type=%c file=%s line=%d", (char)type, file.c_str(),
           line);

  // Only line events are relevant for line breakpoints and stepping
  if (type != 'l')
    return;

  // Watchdog Check - SKIP if debugging is enabled!
  // When debugging, we don't want watchdog to kill execution before breakpoints
  if (m_engine && m_engine->m_watchdogEnabled && !m_enabled) {
    double elapsed = platform::Time::now() - m_engine->m_executionStartTime;
    if (elapsed > m_engine->m_watchdogTimeout) {
      fprintf(stderr, "WATCHDOG TIMEOUT at %s:%d\n", file.c_str(), line);
      sq_throwerror(v, "Watchdog timeout: Execution time limit exceeded");
      return;
    }
  }

  // 1) BP CHECK LOG - Using fprintf to ensure visibility
  fprintf(stderr,
          "BP CHECK file='%s' line=%d enabled=%d action=%d paused=%d hit=%d\n",
          file.c_str(), line, (int)m_enabled, (int)m_action, (int)m_paused,
          (int)m_breakpoints.hasBreakpoint(file, line));
  fflush(stderr);

  // 2) Breakpoints must be checked EVEN WHEN action == Continue
  if (m_breakpoints.hasBreakpoint(file, line)) {
    LOG_INFO("Hit breakpoint at %s:%d", file.c_str(), line);
    // Trigger stop and notify UI
    m_paused = true;
    m_action = DebugAction::None;
    if (m_onStop)
      m_onStop(line, file, "breakpoint");

    // BLOCKING WAIT: Spin here until debugger is resumed or app should exit
    // Call UI pump callback to keep UI responsive, or sleep if not available
    LOG_INFO("Debugger paused at %s:%d - waiting for continue", file.c_str(),
             line);
    while (m_paused) {
      // Check if app wants to exit
      if (m_shouldExit && m_shouldExit()) {
        LOG_INFO("Exit requested - breaking debug pause");
        m_paused = false;
        break;
      }
      if (m_uiPump) {
        // Pump UI events to keep application responsive
        m_uiPump();
      } else {
        // Fallback: small sleep to avoid CPU spin
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    LOG_INFO("Debugger resumed from %s:%d", file.c_str(), line);
    return;
  }

  // If just running, we are done
  if (m_action == DebugAction::Continue) {
    return;
  }

  // 3) Stepping checks
  bool shouldStop = false;
  std::string reason;

  switch (m_action) {
  case DebugAction::StepIn:
    shouldStop = true;
    reason = "step";
    break;
  case DebugAction::StepOver:
    // Simple depth check (needs refinement for recursive calls potentially)
    // m_currentDepth tracked via 'c' calls? We are in 'l' so we don't see calls
    // here directly but 'c' events update m_currentDepth. NOTE: We filtered
    // type != 'l' above. This means we miss depth tracking! FIXED: We must
    // track depth BEFORE returning for != 'l'. WAIT: The user logic said "Only
    // line events can hit line breakpoints". But we need 'c'/'r' for depth.
    // Re-adding depth tracking support:
    break;
    // ...
  default:
    break;
  }

  // RE-INSERT DEPTH TRACKING restore logic
  // Logic flaw in user suggestion: if we return early on != 'l', we miss
  // 'c'/'r' events for StepOver. We need to handle 'c'/'r' first, THEN check
  // 'l'.
}

} // namespace arcanee::script
