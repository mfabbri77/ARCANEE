#include "DapClient.h"
#include "script/ScriptEngine.h"
#include <algorithm>

namespace arcanee::ide {

DapClient::DapClient() {}

DapClient::~DapClient() { Disconnect(); }

std::string DapClient::ToVfsPath(const std::string &hostPath) const {
  if (m_projectRoot.empty() || hostPath.empty()) {
    return hostPath;
  }

  // Basic normalization: convert host path to VFS path
  if (hostPath.find(m_projectRoot) == 0) {
    std::string relative = hostPath.substr(m_projectRoot.length());
    if (relative.length() > 0 && relative[0] == '/') {
      relative = relative.substr(1);
    }
    return "cart:/" + relative;
  } else if (hostPath.find("cart:/") == 0) {
    return hostPath; // Already VFS path
  }

  return hostPath;
}

std::string DapClient::ToHostPath(const std::string &vfsPath) const {
  if (m_projectRoot.empty() || vfsPath.empty())
    return vfsPath;

  if (vfsPath.find("cart:/") == 0) {
    std::string relative = vfsPath.substr(6);
    return m_projectRoot + "/" + relative;
  }

  return vfsPath;
}

void DapClient::SetScriptEngine(script::ScriptEngine *engine) {
  m_scriptEngine = engine;
  if (m_scriptEngine) {
    m_scriptEngine->setOnDebugStop(
        [this](int line, const std::string &file, const std::string &reason) {
          // Capture data and callbacks under lock
          std::string hostFile;
          OutputCallback outputCb;
          StoppedCallback stoppedCb;
          {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = DebugState::Stopped;
            hostFile = ToHostPath(file);

            // Update callstack immediately so UI can show it
            if (m_scriptEngine) {
              auto stack = m_scriptEngine->getCallStack();
              m_callStack.clear();
              int idCounter = 0;
              for (const auto &frame : stack) {
                m_callStack.push_back({idCounter++, frame.name,
                                       ToHostPath(frame.file), frame.line});
              }
              if (!m_callStack.empty()) {
                m_currentScript = m_callStack[0].file;
              }
            }
            outputCb = m_onOutput;
            stoppedCb = m_onStopped;
          }
          // Call callbacks OUTSIDE lock to avoid reentrancy deadlock
          if (outputCb) {
            outputCb("console", "[DAP] Stopped at " + file + ":" +
                                    std::to_string(line) + " (" + reason + ")");
          }
          if (stoppedCb) {
            stoppedCb(reason, line, hostFile);
          }
        });
  }
}

bool DapClient::Launch(const std::string &scriptPath) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != DebugState::Disconnected) {
    return false;
  }

  m_currentScript = scriptPath;
  m_callStack.clear();
  m_locals.clear();

  if (m_onOutput) {
    m_onOutput("console", "[DAP] Launched debug session for: " + scriptPath);
  }

  // If we have a ScriptEngine, use real debugging
  if (m_scriptEngine) {
    fprintf(stderr, "DapClient::Launch: Syncing %zu breakpoints...\n",
            m_breakpoints.size());
    // Sync breakpoints to ScriptEngine
    m_scriptEngine->clearBreakpoints();
    for (const auto &bp : m_breakpoints) {
      if (bp.enabled) {
        m_scriptEngine->addBreakpoint(ToVfsPath(bp.file), bp.line);
      }
    }

    // Enable Debugging!
    m_scriptEngine->setDebugEnabled(true);

    // Update state to reflect we are running/debugging
    m_state = DebugState::Running;

    // We do NOT pause on entry by default anymore.
    // We let it run until it hits a breakpoint or user pauses.

  } else {
    // Simulated debugging
    m_state = DebugState::Running;
    SimulateStop("entry", 1, m_currentScript);
  }

  return true;
}

void DapClient::Disconnect() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_scriptEngine) {
    m_scriptEngine->setDebugEnabled(false);
    m_scriptEngine->setOnDebugStop(nullptr);
  }

  m_state = DebugState::Disconnected;
  m_callStack.clear();
  m_locals.clear();
  m_currentScript.clear();
}

void DapClient::Continue() {
  script::ScriptEngine *engine = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != DebugState::Stopped) {
      return;
    }
    if (m_onOutput) {
      m_onOutput("console", "[DAP] Continuing...");
    }
    engine = m_scriptEngine;
    m_state = DebugState::Running;
  }
  // Mutex released BEFORE calling engine to prevent deadlock
  if (engine) {
    engine->setDebugAction(script::DebugAction::Continue);
  } else {
    // Fallback simulation (re-acquire lock for state mutation)
    std::lock_guard<std::mutex> lock(m_mutex);
    bool hitBreakpoint = false;
    for (const auto &bp : m_breakpoints) {
      if (bp.enabled && bp.file == m_currentScript) {
        SimulateStop("breakpoint", bp.line, bp.file);
        hitBreakpoint = true;
        break;
      }
    }
    if (!hitBreakpoint) {
      m_state = DebugState::Terminated;
      if (m_onOutput) {
        m_onOutput("console", "[DAP] Program terminated.");
      }
    }
  }
}

void DapClient::StepIn() {
  script::ScriptEngine *engine = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != DebugState::Stopped) {
      return;
    }
    if (m_onOutput) {
      m_onOutput("console", "[DAP] Step In");
    }
    engine = m_scriptEngine;
    m_state = DebugState::Running;
  }
  // Mutex released BEFORE calling engine to prevent deadlock
  if (engine) {
    engine->setDebugAction(script::DebugAction::StepIn);
  } else {
    // Fallback simulation
    std::lock_guard<std::mutex> lock(m_mutex);
    int currentLine = m_callStack.empty() ? 1 : m_callStack[0].line;
    std::string currentFile =
        m_callStack.empty() ? m_currentScript : m_callStack[0].file;
    SimulateStop("step", currentLine + 1, currentFile);
  }
}

void DapClient::StepOver() {
  script::ScriptEngine *engine = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != DebugState::Stopped) {
      return;
    }
    if (m_onOutput) {
      m_onOutput("console", "[DAP] Step Over");
    }
    engine = m_scriptEngine;
    m_state = DebugState::Running;
  }
  // Mutex released BEFORE calling engine to prevent deadlock
  if (engine) {
    engine->setDebugAction(script::DebugAction::StepOver);
  } else {
    // Fallback simulation
    std::lock_guard<std::mutex> lock(m_mutex);
    int currentLine = m_callStack.empty() ? 1 : m_callStack[0].line;
    std::string currentFile =
        m_callStack.empty() ? m_currentScript : m_callStack[0].file;
    SimulateStop("step", currentLine + 1, currentFile);
  }
}

void DapClient::StepOut() {
  script::ScriptEngine *engine = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != DebugState::Stopped) {
      return;
    }
    if (m_onOutput) {
      m_onOutput("console", "[DAP] Step Out");
    }
    engine = m_scriptEngine;
    m_state = DebugState::Running;
  }
  // Mutex released BEFORE calling engine to prevent deadlock
  if (engine) {
    engine->setDebugAction(script::DebugAction::StepOut);
  } else {
    // Fallback simulation
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_callStack.size() > 1) {
      m_callStack.erase(m_callStack.begin());
      SimulateStop("step", m_callStack[0].line, m_callStack[0].file);
    } else {
      int currentLine = m_callStack.empty() ? 1 : m_callStack[0].line;
      std::string currentFile =
          m_callStack.empty() ? m_currentScript : m_callStack[0].file;
      SimulateStop("step", currentLine + 1, currentFile);
    }
  }
}

void DapClient::Pause() {
  script::ScriptEngine *engine = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != DebugState::Running) {
      return;
    }
    engine = m_scriptEngine;
  }
  // Mutex released BEFORE calling engine to prevent deadlock
  if (engine) {
    // Use real Pause action that stops on next line event
    engine->setDebugAction(script::DebugAction::Pause);
  } else {
    std::lock_guard<std::mutex> lock(m_mutex);
    SimulateStop("pause", 1, m_currentScript);
  }
}

void DapClient::Stop() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_scriptEngine) {
    m_scriptEngine->terminate();
  }

  m_state = DebugState::Disconnected;
  m_callStack.clear();
  m_locals.clear();
  if (m_onOutput) {
    m_onOutput("console", "[DAP] Debug session stopped.");
  }
}

void DapClient::SetBreakpoint(const std::string &file, int line) {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::string vfsPath = ToVfsPath(file);
  fprintf(stderr, "DapClient::SetBreakpoint Host=%s VFS=%s:%d\n", file.c_str(),
          vfsPath.c_str(), line);

  // Check if already exists (Compare VFS path)
  for (auto &bp : m_breakpoints) {
    if (bp.file == vfsPath && bp.line == line) {
      bp.enabled = true;
      // Sync to ScriptEngine (It expects VFS path)
      if (m_scriptEngine) {
        m_scriptEngine->addBreakpoint(vfsPath, line);
      }
      return;
    }
  }

  BreakpointInfo bp;
  bp.file = vfsPath; // STORE VFS PATH
  bp.line = line;
  bp.enabled = true;
  bp.id = m_nextBreakpointId++;
  m_breakpoints.push_back(bp);

  // Sync to ScriptEngine
  if (m_scriptEngine) {
    m_scriptEngine->addBreakpoint(vfsPath, line);
  } else {
    fprintf(stderr, "DapClient: ScriptEngine is NULL, stored BP locally.\n");
  }
}

void DapClient::RemoveBreakpoint(const std::string &file, int line) {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::string vfsPath = ToVfsPath(file);
  fprintf(stderr, "DapClient::RemoveBreakpoint Host=%s VFS=%s:%d\n",
          file.c_str(), vfsPath.c_str(), line);

  // Remove from local list (Compare VFS path)
  m_breakpoints.erase(std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
                                     [&](const BreakpointInfo &bp) {
                                       return bp.file == vfsPath &&
                                              bp.line == line;
                                     }),
                      m_breakpoints.end());

  // Sync to ScriptEngine
  if (m_scriptEngine) {
    m_scriptEngine->removeBreakpoint(vfsPath, line);
  }
}

void DapClient::ToggleBreakpoint(const std::string &file, int line) {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::string vfsPath = ToVfsPath(file);

  // Check if exists - if so, remove it
  for (auto it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it) {
    if (it->file == vfsPath && it->line == line) {
      m_breakpoints.erase(it);
      if (m_scriptEngine) {
        m_scriptEngine->removeBreakpoint(vfsPath, line);
      }
      return;
    }
  }

  // Not found, add it
  BreakpointInfo bp;
  bp.file = vfsPath; // STORE VFS PATH
  bp.line = line;
  bp.enabled = true;
  bp.id = m_nextBreakpointId++;
  m_breakpoints.push_back(bp);

  if (m_scriptEngine) {
    m_scriptEngine->addBreakpoint(vfsPath, line);
  }
}

std::vector<BreakpointInfo> DapClient::GetBreakpoints() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  // Return HOST paths for UI
  std::vector<BreakpointInfo> result;
  result.reserve(m_breakpoints.size());
  for (const auto &bp : m_breakpoints) {
    BreakpointInfo hostBp = bp;
    // const_cast because ToHostPath isn't const? It should be.
    // DapClient::ToHostPath is a member func.
    // But we are in a const function. ToHostPath must be const!
    // Oops, in the viewed file ToHostPath(const std::string&) has no const
    // qualifier on method. I can't call it easily without const_cast on 'this'
    // or making it static/const. I'll assume I can just use
    // const_cast<DapClient*>(this)->ToHostPath or fix ToHostPath signature.
    // Let's use const_cast for minimal impact now, or duplicate logic.
    // Or better: ToHostPath doesn't modify state.
    hostBp.file = const_cast<DapClient *>(this)->ToHostPath(bp.file);
    result.push_back(hostBp);
  }
  return result;
}

std::vector<StackFrame> DapClient::GetCallStack() {
  std::lock_guard<std::mutex> lock(m_mutex);

  // If we have a ScriptEngine and are stopped, get real call stack
  if (m_scriptEngine && m_state == DebugState::Stopped) {
    auto realStack = m_scriptEngine->getCallStack();
    std::vector<StackFrame> result;
    for (const auto &frame : realStack) {
      // Convert VFS path to host path for UI compatibility
      result.push_back(
          {frame.id, frame.name, ToHostPath(frame.file), frame.line});
    }
    return result;
  }

  return m_callStack;
}

std::vector<Variable> DapClient::GetLocals(int frameId) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // If we have a ScriptEngine and are stopped, get real locals
  if (m_scriptEngine && m_state == DebugState::Stopped) {
    auto realLocals = m_scriptEngine->getLocals(frameId);
    std::vector<Variable> result;
    for (const auto &var : realLocals) {
      result.push_back({var.name, var.value, var.type, 0});
    }
    return result;
  }

  return m_locals;
}

std::vector<Variable> DapClient::GetGlobals() {
  std::lock_guard<std::mutex> lock(m_mutex);
  // TODO: Implement real globals from ScriptEngine
  return {{"_version", "\"1.0\"", "string"}, {"DEBUG", "true", "bool"}};
}

void DapClient::SimulateStop(const std::string &reason, int line,
                             const std::string &file) {
  m_state = DebugState::Stopped;

  // Create simulated call stack
  m_callStack.clear();
  m_callStack.push_back({0, "main", file, line});
  if (line > 10) {
    m_callStack.push_back({1, "helper", file, line - 5});
  }

  // Create simulated locals that change based on line
  m_locals.clear();
  m_locals.push_back({"line", std::to_string(line), "integer"});
  m_locals.push_back({"x", std::to_string(line * 10), "integer"});
  m_locals.push_back(
      {"name", "\"step_" + std::to_string(line) + "\"", "string"});

  if (m_onStopped) {
    m_onStopped(reason, line, file);
  }
}

} // namespace arcanee::ide
