#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace arcanee::ide {

enum class DiagnosticSeverity {
  Error = 1,
  Warning = 2,
  Information = 3,
  Hint = 4
};

struct Diagnostic {
  std::string file;
  int line;
  int column;
  DiagnosticSeverity severity;
  std::string message;
  std::string source = "squirrel";
};

class LspClient {
public:
  LspClient();
  ~LspClient();

  // Initialize (MVP: in-process mock)
  bool Initialize();
  void Shutdown();

  // Document notifications
  void DidOpen(const std::string &file, const std::string &content);
  void DidChange(const std::string &file, const std::string &content);
  void DidClose(const std::string &file);

  // Get diagnostics for all files
  std::vector<Diagnostic> GetDiagnostics() const;

  // Get diagnostics for specific file
  std::vector<Diagnostic> GetDiagnostics(const std::string &file) const;

private:
  void AnalyzeSquirrel(const std::string &file, const std::string &content);

  mutable std::mutex m_mutex;
  std::vector<Diagnostic> m_diagnostics;
  bool m_initialized = false;
};

} // namespace arcanee::ide
