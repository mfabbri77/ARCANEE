#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace arcanee::ide {

struct TaskDefinition {
  std::string name;
  std::string command;
  std::string workingDir;
  std::string problemMatcher; // "gcc", "msvc", "generic", or empty
};

struct ProblemMatch {
  std::string file;
  int line;
  int column;
  std::string severity; // "error", "warning", "info"
  std::string message;
};

class TaskRunner {
public:
  TaskRunner();
  ~TaskRunner();

  // Load tasks from tasks.toml
  bool LoadTasks(const std::string &projectRoot);

  // Get available tasks
  const std::vector<TaskDefinition> &GetTasks() const { return m_tasks; }

  // Run a task by name
  bool RunTask(const std::string &name);

  // Check if a task is running
  bool IsRunning() const { return m_running; }

  // Get output lines (thread-safe copy)
  std::vector<std::string> GetOutput();

  // Get problem matches
  std::vector<ProblemMatch> GetProblems();

  // Cancel current task
  void Cancel();

private:
  void ExecuteTask(TaskDefinition task);
  void ParseProblemLine(const std::string &line, const std::string &matcher);

  std::vector<TaskDefinition> m_tasks;
  std::thread m_worker;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_cancel{false};

  std::mutex m_outputMutex;
  std::vector<std::string> m_output;
  std::vector<ProblemMatch> m_problems;
  std::string m_currentMatcher;
};

} // namespace arcanee::ide
