#pragma once
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace arcanee::ide {

struct BreakpointInfo {
  std::string file;
  int line;
  bool enabled = true;
  int id = -1; // Assigned by adapter
};

struct StackFrame {
  int id;
  std::string name;
  std::string file;
  int line;
};

struct Variable {
  std::string name;
  std::string value;
  std::string type;
  int variablesReference = 0; // For expandable objects
};

enum class DebugState { Disconnected, Running, Stopped, Terminated };

class DapClient {
public:
  DapClient();
  ~DapClient();

  // Session management
  bool Launch(const std::string &scriptPath);
  void Disconnect();
  DebugState GetState() const { return m_state; }

  // Execution control
  void Continue();
  void StepIn();
  void StepOver();
  void StepOut();
  void Pause();
  void Stop();

  // Breakpoints
  void SetBreakpoint(const std::string &file, int line);
  void RemoveBreakpoint(const std::string &file, int line);
  void ToggleBreakpoint(const std::string &file, int line);
  std::vector<BreakpointInfo> GetBreakpoints() const;

  // Inspection
  std::vector<StackFrame> GetCallStack();
  std::vector<Variable> GetLocals(int frameId);
  std::vector<Variable> GetGlobals();

  // Callbacks
  using StoppedCallback = std::function<void(
      const std::string &reason, int line, const std::string &file)>;
  using OutputCallback = std::function<void(const std::string &category,
                                            const std::string &output)>;
  void SetOnStopped(StoppedCallback cb) { m_onStopped = cb; }
  void SetOnOutput(OutputCallback cb) { m_onOutput = cb; }

private:
  // MVP: In-process debug simulation
  void SimulateStop(const std::string &reason, int line,
                    const std::string &file);

  DebugState m_state = DebugState::Disconnected;
  std::vector<BreakpointInfo> m_breakpoints;
  std::vector<StackFrame> m_callStack;
  std::vector<Variable> m_locals;

  StoppedCallback m_onStopped;
  OutputCallback m_onOutput;

  mutable std::mutex m_mutex;
  int m_nextBreakpointId = 1;
  std::string m_currentScript;
};

} // namespace arcanee::ide
