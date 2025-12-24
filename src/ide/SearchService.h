#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace arcanee::ide {

struct SearchMatch {
  std::string filePath;
  int lineNumber; // 1-based
  std::string lineContent;
  // int colStart; // For highlighting in results - later
  // int colEnd;
};

struct SearchResult {
  std::string query;
  bool isRegex;
  bool caseSensitive;
  std::vector<SearchMatch> matches;
  bool complete = false;
};

class SearchService {
public:
  SearchService();
  ~SearchService();

  // Start a new search. Cancels any existing search.
  void StartSearch(const std::string &rootPath, const std::string &query,
                   bool isRegex, bool caseSensitive);

  // Stop current search
  void CancelSearch();

  // Poll current results (thread-safe copy)
  SearchResult GetResults();

private:
  void SearchWorker(std::string rootPath, std::string query, bool isRegex,
                    bool caseSensitive);

  std::thread m_worker;
  std::atomic<bool> m_cancel{false};
  std::mutex m_mutex;
  SearchResult m_currentResult;
};

} // namespace arcanee::ide
