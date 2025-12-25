#include "DapClient.h"
#include <algorithm>

namespace arcanee::ide {

DapClient::DapClient() {}

DapClient::~DapClient() { Disconnect(); }

bool DapClient::Launch(const std::string &scriptPath) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != DebugState::Disconnected) {
    return false;
  }

  m_currentScript = scriptPath;
  m_state = DebugState::Running;
  m_callStack.clear();
  m_locals.clear();

  if (m_onOutput) {
    m_onOutput("console", "[DAP] Launched debug session for: " + scriptPath);
  }

  // MVP: Simulate immediate stop at first line for testing
  // In real implementation, this would connect to Squirrel VM debug hooks
  SimulateStop("entry", 1, scriptPath);

  return true;
}

void DapClient::Disconnect() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_state = DebugState::Disconnected;
  m_callStack.clear();
  m_locals.clear();
  m_currentScript.clear();
}

void DapClient::Continue() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state == DebugState::Stopped) {
    m_state = DebugState::Running;
    if (m_onOutput) {
      m_onOutput("console", "[DAP] Continuing...");
    }
    // MVP: Simulate hitting next breakpoint or termination
    // Check if there are more breakpoints
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
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state == DebugState::Stopped) {
    if (m_onOutput) {
      m_onOutput("console", "[DAP] Step In");
    }
    // MVP: Simulate stepping to next line
    int currentLine = m_callStack.empty() ? 1 : m_callStack[0].line;
    std::string currentFile =
        m_callStack.empty() ? m_currentScript : m_callStack[0].file;
    SimulateStop("step", currentLine + 1, currentFile);
  }
}

void DapClient::StepOver() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state == DebugState::Stopped) {
    if (m_onOutput) {
      m_onOutput("console", "[DAP] Step Over");
    }
    int currentLine = m_callStack.empty() ? 1 : m_callStack[0].line;
    std::string currentFile =
        m_callStack.empty() ? m_currentScript : m_callStack[0].file;
    SimulateStop("step", currentLine + 1, currentFile);
  }
}

void DapClient::StepOut() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state == DebugState::Stopped) {
    if (m_onOutput) {
      m_onOutput("console", "[DAP] Step Out");
    }
    // MVP: If multiple frames, pop one. Otherwise just step.
    if (m_callStack.size() > 1) {
      m_callStack.erase(m_callStack.begin());
      SimulateStop("step", m_callStack[0].line, m_callStack[0].file);
    } else {
      // Just step to next line
      int currentLine = m_callStack.empty() ? 1 : m_callStack[0].line;
      std::string currentFile =
          m_callStack.empty() ? m_currentScript : m_callStack[0].file;
      SimulateStop("step", currentLine + 1, currentFile);
    }
  }
}

void DapClient::Pause() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state == DebugState::Running) {
    SimulateStop("pause", 1, m_currentScript);
  }
}

void DapClient::Stop() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_state = DebugState::Terminated;
  m_callStack.clear();
  m_locals.clear();
  if (m_onOutput) {
    m_onOutput("console", "[DAP] Debug session stopped.");
  }
}

void DapClient::SetBreakpoint(const std::string &file, int line) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Check if already exists
  for (auto &bp : m_breakpoints) {
    if (bp.file == file && bp.line == line) {
      bp.enabled = true;
      return;
    }
  }

  BreakpointInfo bp;
  bp.file = file;
  bp.line = line;
  bp.enabled = true;
  bp.id = m_nextBreakpointId++;
  m_breakpoints.push_back(bp);
}

void DapClient::RemoveBreakpoint(const std::string &file, int line) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_breakpoints.erase(std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
                                     [&](const BreakpointInfo &bp) {
                                       return bp.file == file &&
                                              bp.line == line;
                                     }),
                      m_breakpoints.end());
}

void DapClient::ToggleBreakpoint(const std::string &file, int line) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Check if exists - if so, remove it
  for (auto it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it) {
    if (it->file == file && it->line == line) {
      m_breakpoints.erase(it);
      return;
    }
  }

  // Not found, add it
  BreakpointInfo bp;
  bp.file = file;
  bp.line = line;
  bp.enabled = true;
  bp.id = m_nextBreakpointId++;
  m_breakpoints.push_back(bp);
}

std::vector<BreakpointInfo> DapClient::GetBreakpoints() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_breakpoints;
}

std::vector<StackFrame> DapClient::GetCallStack() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_callStack;
}

std::vector<Variable> DapClient::GetLocals(int frameId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  // MVP: Return simulated locals
  return m_locals;
}

std::vector<Variable> DapClient::GetGlobals() {
  std::lock_guard<std::mutex> lock(m_mutex);
  // MVP: Return simulated globals
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
