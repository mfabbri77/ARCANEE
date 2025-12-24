#pragma once
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Forward declaration from tree-sitter
typedef struct TSLanguage TSLanguage;
typedef struct TSParser TSParser;
typedef struct TSTree TSTree;
typedef struct TSQuery TSQuery;

namespace arcanee::ide {

struct Document;

struct HighlightSpan {
  uint32_t startByte;
  uint32_t endByte;
  uint32_t color; // packed RGBA or palette index
};

// Represents syntax highlighting for a specific version of a document
struct ParseResult {
  std::vector<HighlightSpan> highlights;
  int revision = 0;
};

class ParseService {
public:
  ParseService();
  ~ParseService();

  void Initialize();
  void Shutdown();

  // Trigger a parse for the given document (async)
  // Pass content explicitly to avoid threading issues with doc buffer
  void UpdateDocument(Document *doc, const std::string &content, int revision);

  // Get latest available highlights
  const ParseResult *GetHighlights(const std::string &docPath);

private:
  void WorkerLoop();
  void PerformParse(const std::string &path, const std::string &content,
                    int revision);

  // Core
  TSParser *m_parser = nullptr;
  TSQuery *m_query = nullptr;
  const TSLanguage *m_lang = nullptr;

  struct DocState {
    TSTree *tree = nullptr;
    ParseResult latestResult;
  };

  std::map<std::string, DocState>
      m_docStates; // Access guarded by mutex?
                   // Ideally, parsing happens on worker, UI reads result.

  // Threading
  std::thread m_workerThread;
  std::mutex m_mutex; // Protects queue and results
  std::condition_variable m_cv;
  bool m_running = false;

  struct ParseJob {
    std::string path;
    std::string content;
    int revision;
  };
  std::vector<ParseJob> m_queue;

  void LoadQuery();
};

} // namespace arcanee::ide
