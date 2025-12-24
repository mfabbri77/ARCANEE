#include "LspClient.h"
#include <algorithm>
#include <regex>
#include <sstream>

namespace arcanee::ide {

LspClient::LspClient() {}

LspClient::~LspClient() { Shutdown(); }

bool LspClient::Initialize() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_initialized = true;
  m_diagnostics.clear();
  return true;
}

void LspClient::Shutdown() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_initialized = false;
  m_diagnostics.clear();
}

void LspClient::DidOpen(const std::string &file, const std::string &content) {
  if (!m_initialized)
    return;
  AnalyzeSquirrel(file, content);
}

void LspClient::DidChange(const std::string &file, const std::string &content) {
  if (!m_initialized)
    return;
  AnalyzeSquirrel(file, content);
}

void LspClient::DidClose(const std::string &file) {
  std::lock_guard<std::mutex> lock(m_mutex);
  // Remove diagnostics for this file
  m_diagnostics.erase(
      std::remove_if(m_diagnostics.begin(), m_diagnostics.end(),
                     [&](const Diagnostic &d) { return d.file == file; }),
      m_diagnostics.end());
}

std::vector<Diagnostic> LspClient::GetDiagnostics() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_diagnostics;
}

std::vector<Diagnostic>
LspClient::GetDiagnostics(const std::string &file) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::vector<Diagnostic> result;
  for (const auto &d : m_diagnostics) {
    if (d.file == file) {
      result.push_back(d);
    }
  }
  return result;
}

void LspClient::AnalyzeSquirrel(const std::string &file,
                                const std::string &content) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Remove old diagnostics for this file
  m_diagnostics.erase(
      std::remove_if(m_diagnostics.begin(), m_diagnostics.end(),
                     [&](const Diagnostic &d) { return d.file == file; }),
      m_diagnostics.end());

  // MVP: Basic syntax checks using simple patterns
  std::istringstream stream(content);
  std::string line;
  int lineNum = 0;

  int braceCount = 0;
  int parenCount = 0;
  int bracketCount = 0;

  while (std::getline(stream, line)) {
    lineNum++;

    // Count braces for balance checking
    for (char c : line) {
      if (c == '{')
        braceCount++;
      else if (c == '}')
        braceCount--;
      else if (c == '(')
        parenCount++;
      else if (c == ')')
        parenCount--;
      else if (c == '[')
        bracketCount++;
      else if (c == ']')
        bracketCount--;
    }

    // Check for unmatched closing
    if (braceCount < 0) {
      Diagnostic d;
      d.file = file;
      d.line = lineNum;
      d.column = 1;
      d.severity = DiagnosticSeverity::Error;
      d.message = "Unmatched closing brace '}'";
      m_diagnostics.push_back(d);
      braceCount = 0;
    }
    if (parenCount < 0) {
      Diagnostic d;
      d.file = file;
      d.line = lineNum;
      d.column = 1;
      d.severity = DiagnosticSeverity::Error;
      d.message = "Unmatched closing parenthesis ')'";
      m_diagnostics.push_back(d);
      parenCount = 0;
    }
    if (bracketCount < 0) {
      Diagnostic d;
      d.file = file;
      d.line = lineNum;
      d.column = 1;
      d.severity = DiagnosticSeverity::Error;
      d.message = "Unmatched closing bracket ']'";
      m_diagnostics.push_back(d);
      bracketCount = 0;
    }

    // Check for common Squirrel issues
    // Empty function body warning
    if (line.find("function") != std::string::npos &&
        line.find("{}") != std::string::npos) {
      Diagnostic d;
      d.file = file;
      d.line = lineNum;
      d.column = 1;
      d.severity = DiagnosticSeverity::Warning;
      d.message = "Empty function body";
      m_diagnostics.push_back(d);
    }

    // TODO comment hint
    if (line.find("TODO") != std::string::npos ||
        line.find("FIXME") != std::string::npos) {
      Diagnostic d;
      d.file = file;
      d.line = lineNum;
      d.column = 1;
      d.severity = DiagnosticSeverity::Information;
      d.message = "TODO/FIXME comment found";
      m_diagnostics.push_back(d);
    }
  }

  // Check for unclosed braces at end of file
  if (braceCount > 0) {
    Diagnostic d;
    d.file = file;
    d.line = lineNum;
    d.column = 1;
    d.severity = DiagnosticSeverity::Error;
    d.message =
        "Unclosed brace(s) - missing " + std::to_string(braceCount) + " '}'";
    m_diagnostics.push_back(d);
  }
  if (parenCount > 0) {
    Diagnostic d;
    d.file = file;
    d.line = lineNum;
    d.column = 1;
    d.severity = DiagnosticSeverity::Error;
    d.message =
        "Unclosed parenthesis - missing " + std::to_string(parenCount) + " ')'";
    m_diagnostics.push_back(d);
  }
  if (bracketCount > 0) {
    Diagnostic d;
    d.file = file;
    d.line = lineNum;
    d.column = 1;
    d.severity = DiagnosticSeverity::Error;
    d.message =
        "Unclosed bracket - missing " + std::to_string(bracketCount) + " ']'";
    m_diagnostics.push_back(d);
  }
}

} // namespace arcanee::ide
