#include "ParseService.h"
#include "DocumentSystem.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <tree_sitter/api.h>

// Declare the external grammar function
extern "C" const TSLanguage *tree_sitter_squirrel();

namespace arcanee::ide {

ParseService::ParseService() {}

ParseService::~ParseService() { Shutdown(); }

void ParseService::Initialize() {
  m_parser = ts_parser_new();
  m_lang = tree_sitter_squirrel();
  ts_parser_set_language(m_parser, m_lang);

  LoadQuery();

  m_running = true;
  m_workerThread = std::thread(&ParseService::WorkerLoop, this);
}

void ParseService::Shutdown() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running)
      return;
    m_running = false;
  }
  m_cv.notify_all();
  if (m_workerThread.joinable())
    m_workerThread.join();

  if (m_query)
    ts_query_delete(m_query);
  if (m_parser)
    ts_parser_delete(m_parser);

  // Cleanup trees
  for (auto &pair : m_docStates) {
    if (pair.second.tree)
      ts_tree_delete(pair.second.tree);
  }
  m_docStates.clear();
}

void ParseService::LoadQuery() {
  // Load query from assets
  // Hardcoded path for MVP, ideally ProjectSystem/AssetManager resolves this
  // Assuming running from bin/
  std::ifstream f("assets/ide/treesitter/squirrel/queries/highlights.scm");
  if (!f.is_open()) {
    std::cerr << "[ParseService] Failed to load highlights.scm" << std::endl;
    return;
  }
  std::stringstream buffer;
  buffer << f.rdbuf();
  std::string source = buffer.str();

  uint32_t errorOffset;
  TSQueryError errorType;
  m_query = ts_query_new(m_lang, source.c_str(), source.length(), &errorOffset,
                         &errorType);

  if (!m_query) {
    std::cerr << "[ParseService] Query compile failed at offset " << errorOffset
              << " type " << errorType << std::endl;
  }
}

void ParseService::UpdateDocument(Document *doc, const std::string &content,
                                  int revision) {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Overwrite existing job for same path to avoid backlog
    for (auto it = m_queue.begin(); it != m_queue.end();) {
      if (it->path == doc->path) {
        it = m_queue.erase(it);
      } else {
        ++it;
      }
    }
    m_queue.push_back({doc->path, content, revision});
  }
  m_cv.notify_one();
}

const ParseResult *ParseService::GetHighlights(const std::string &docPath) {
  // Unsafe access, but needed for MVP speed in UI
  // Assuming UI reads are frequent, writes are from worker
  // A strict implementation would copy.
  // Let's implement valid lock access in `UIShell` side or return copy.
  // For now: lock and return reference valid only during lock? No.
  // Return pointer to map entry?
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_docStates.find(docPath);
  if (it != m_docStates.end()) {
    return &it->second.latestResult;
  }
  return nullptr;
}

void ParseService::WorkerLoop() {
  while (true) {
    ParseJob job;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });

      if (!m_running)
        break;

      if (!m_queue.empty()) {
        job = m_queue.front();
        m_queue.erase(m_queue.begin());
      }
    }
    if (!job.path.empty())
      PerformParse(job.path, job.content, job.revision);
  }
}

void ParseService::PerformParse(const std::string &path,
                                const std::string &content, int revision) {
  // 1. Parse
  // Incremental not implemented yet, just reparse full
  TSTree *oldTree = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_docStates[path].tree)
      oldTree = m_docStates[path].tree;
  }

  // Pass oldTree if we had edits, but we just reparse from string
  // To support incremental, we need to pass edit ops to tree-sitter.
  // Simple reparse:
  TSTree *newTree = ts_parser_parse_string(m_parser, oldTree, content.c_str(),
                                           content.length());

  // 2. Query
  if (newTree && m_query) {
    TSQueryCursor *cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, m_query, ts_tree_root_node(newTree));

    std::vector<HighlightSpan> highlights;
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
      for (int i = 0; i < match.capture_count; i++) {
        TSQueryCapture capture = match.captures[i];
        uint32_t start = ts_node_start_byte(capture.node);
        uint32_t end = ts_node_end_byte(capture.node);

        // Color mapping logic
        // Currently captures are indices into query pattern names
        // We'll trust index matches pattern order or get name
        uint32_t len;
        const char *name =
            ts_query_capture_name_for_id(m_query, capture.index, &len);
        std::string captureName(name, len);

        uint32_t color = 0xFFFFFFFF; // White default
        if (captureName == "keyword")
          color = 0xFF569CD6; // Blue
        else if (captureName == "string")
          color = 0xFFCE9178; // Orange/Red
        else if (captureName == "comment")
          color = 0xFF6A9955; // Green
        else if (captureName == "function")
          color = 0xFFDCDCAA; // Yellow
        else if (captureName == "number")
          color = 0xFFB5CEA8; // Light Green
        else if (captureName == "type")
          color = 0xFF4EC9B0; // Teal

        if (end > start)
          highlights.push_back({start, end, color});
      }
    }

    ts_query_cursor_delete(cursor);

    // 3. Update State
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      // Re-fetch state entry, it might have been touched? No, map stable.
      // Delete old tree
      if (m_docStates[path].tree && m_docStates[path].tree != newTree) {
        ts_tree_delete(m_docStates[path].tree);
      }
      m_docStates[path].tree = newTree;
      m_docStates[path].latestResult.highlights = std::move(highlights);
      m_docStates[path].latestResult.revision = revision;
    }
  } else {
    if (newTree)
      ts_tree_delete(newTree);
  }
}

} // namespace arcanee::ide
