#include "TaskRunner.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <toml++/toml.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace arcanee::ide {

namespace fs = std::filesystem;

TaskRunner::TaskRunner() {}

TaskRunner::~TaskRunner() { Cancel(); }

bool TaskRunner::LoadTasks(const std::string &projectRoot) {
  m_tasks.clear();

  fs::path tasksPath = fs::path(projectRoot) / "tasks.toml";
  if (!fs::exists(tasksPath)) {
    return false;
  }

  try {
    auto tbl = toml::parse_file(tasksPath.string());

    if (auto tasks = tbl["tasks"].as_array()) {
      for (const auto &taskNode : *tasks) {
        if (auto taskTbl = taskNode.as_table()) {
          TaskDefinition def;
          if (auto name = (*taskTbl)["name"].value<std::string>()) {
            def.name = *name;
          }
          if (auto cmd = (*taskTbl)["command"].value<std::string>()) {
            def.command = *cmd;
          }
          if (auto wd = (*taskTbl)["workingDir"].value<std::string>()) {
            def.workingDir = *wd;
          } else {
            def.workingDir = projectRoot;
          }
          if (auto pm = (*taskTbl)["problemMatcher"].value<std::string>()) {
            def.problemMatcher = *pm;
          }

          if (!def.name.empty() && !def.command.empty()) {
            m_tasks.push_back(def);
          }
        }
      }
    }
    return true;
  } catch (const toml::parse_error &e) {
    (void)e;
    return false;
  }
}

bool TaskRunner::RunTask(const std::string &name) {
  if (m_running)
    return false;

  for (const auto &task : m_tasks) {
    if (task.name == name) {
      m_cancel = false;
      m_running = true;

      {
        std::lock_guard<std::mutex> lock(m_outputMutex);
        m_output.clear();
        m_problems.clear();
        m_currentMatcher = task.problemMatcher;
      }

      if (m_worker.joinable()) {
        m_worker.join();
      }
      m_worker = std::thread(&TaskRunner::ExecuteTask, this, task);
      return true;
    }
  }
  return false;
}

std::vector<std::string> TaskRunner::GetOutput() {
  std::lock_guard<std::mutex> lock(m_outputMutex);
  return m_output;
}

std::vector<ProblemMatch> TaskRunner::GetProblems() {
  std::lock_guard<std::mutex> lock(m_outputMutex);
  return m_problems;
}

void TaskRunner::Cancel() {
  m_cancel = true;
  if (m_worker.joinable()) {
    m_worker.join();
  }
  m_running = false;
}

void TaskRunner::ExecuteTask(TaskDefinition task) {
  // Change to working directory
  fs::path origDir = fs::current_path();
  try {
    fs::current_path(task.workingDir);
  } catch (...) {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    m_output.push_back("[Error] Failed to change to working directory: " +
                       task.workingDir);
    m_running = false;
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    m_output.push_back("[Running] " + task.command);
  }

#ifdef _WIN32
  // Windows implementation using popen
  FILE *pipe = _popen(task.command.c_str(), "r");
#else
  FILE *pipe = popen(task.command.c_str(), "r");
#endif

  if (!pipe) {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    m_output.push_back("[Error] Failed to execute command");
    m_running = false;
    fs::current_path(origDir);
    return;
  }

  char buffer[512];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr && !m_cancel) {
    std::string line(buffer);
    // Remove trailing newline
    if (!line.empty() && line.back() == '\n') {
      line.pop_back();
    }
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    {
      std::lock_guard<std::mutex> lock(m_outputMutex);
      m_output.push_back(line);
      ParseProblemLine(line, m_currentMatcher);
    }
  }

#ifdef _WIN32
  int result = _pclose(pipe);
#else
  int result = pclose(pipe);
#endif

  {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    if (m_cancel) {
      m_output.push_back("[Cancelled]");
    } else {
      m_output.push_back("[Finished] Exit code: " + std::to_string(result));
    }
  }

  fs::current_path(origDir);
  m_running = false;
}

void TaskRunner::ParseProblemLine(const std::string &line,
                                  const std::string &matcher) {
  if (matcher.empty())
    return;

  // GCC/Clang format: file:line:column: severity: message
  // Example: main.cpp:10:5: error: expected ';'
  static std::regex gccRegex(
      R"(^(.+):(\d+):(\d+):\s*(error|warning|note):\s*(.+)$)");

  // MSVC format: file(line,column): severity code: message
  // Example: main.cpp(10,5): error C2143: syntax error
  static std::regex msvcRegex(
      R"(^(.+)\((\d+),(\d+)\):\s*(error|warning)\s+\w+:\s*(.+)$)");

  std::smatch match;

  if (matcher == "gcc" || matcher == "generic") {
    if (std::regex_match(line, match, gccRegex)) {
      ProblemMatch pm;
      pm.file = match[1].str();
      pm.line = std::stoi(match[2].str());
      pm.column = std::stoi(match[3].str());
      pm.severity = match[4].str();
      pm.message = match[5].str();
      m_problems.push_back(pm);
      return;
    }
  }

  if (matcher == "msvc" || matcher == "generic") {
    if (std::regex_match(line, match, msvcRegex)) {
      ProblemMatch pm;
      pm.file = match[1].str();
      pm.line = std::stoi(match[2].str());
      pm.column = std::stoi(match[3].str());
      pm.severity = match[4].str();
      pm.message = match[5].str();
      m_problems.push_back(pm);
      return;
    }
  }
}

} // namespace arcanee::ide
