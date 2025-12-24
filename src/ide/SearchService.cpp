#define PCRE2_CODE_UNIT_WIDTH 8
#include "SearchService.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <pcre2.h>

namespace arcanee::ide {

namespace fs = std::filesystem;

SearchService::SearchService() {}

SearchService::~SearchService() { CancelSearch(); }

void SearchService::CancelSearch() {
  m_cancel = true;
  if (m_worker.joinable()) {
    m_worker.join();
  }
}

void SearchService::StartSearch(const std::string &rootPath,
                                const std::string &query, bool isRegex,
                                bool caseSensitive) {
  CancelSearch(); // Stop existing

  if (query.empty())
    return;

  m_cancel = false;
  m_currentResult = SearchResult();
  m_currentResult.query = query;
  m_currentResult.isRegex = isRegex;
  m_currentResult.caseSensitive = caseSensitive;

  m_worker = std::thread(&SearchService::SearchWorker, this, rootPath, query,
                         isRegex, caseSensitive);
}

SearchResult SearchService::GetResults() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_currentResult; // Copy
}

// Helper to check if file is text
bool IsTextFile(const fs::path &path) {
  // Simple extension check for now + size limit?
  // or just try to read first chunk and check for nulls
  // Extensions explicit allowed or ignore binaries
  std::string ext = path.extension().string();
  if (ext == ".png" || ext == ".jpg" || ext == ".obj" || ext == ".exe" ||
      ext == ".dll" || ext == ".so" || ext == ".a" || ext == ".lib")
    return false;
  if (ext == ".git")
    return false;
  return true;
}

void SearchService::SearchWorker(std::string rootPath, std::string query,
                                 bool isRegex, bool caseSensitive) {
  // PCRE2 Setup
  pcre2_code *re = nullptr;
  pcre2_match_data *match_data = nullptr;

  if (isRegex) {
    int errornumber;
    PCRE2_SIZE erroroffset;
    uint32_t options = 0;
    if (!caseSensitive)
      options |= PCRE2_CASELESS;

    re = pcre2_compile((PCRE2_SPTR)query.c_str(), PCRE2_ZERO_TERMINATED,
                       options, &errornumber, &erroroffset, nullptr);

    if (!re) {
      // Compile failed
      // Record error in result? For now just exit
      std::lock_guard<std::mutex> lock(m_mutex);
      m_currentResult.complete = true;
      return;
    }
    match_data = pcre2_match_data_create_from_pattern(re, nullptr);
  } else {
    // String search case handling
    if (!caseSensitive) {
      std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    }
  }

  try {
    for (const auto &entry : fs::recursive_directory_iterator(rootPath)) {
      if (m_cancel)
        break;

      if (entry.is_regular_file() && IsTextFile(entry.path())) {
        std::ifstream file(entry.path());
        std::string line;
        int lineNum = 0;
        std::vector<SearchMatch> fileMatches;

        while (std::getline(file, line)) {
          lineNum++;

          bool found = false;
          if (isRegex) {
            int rc = pcre2_match(re, (PCRE2_SPTR)line.c_str(), line.length(), 0,
                                 0, match_data, nullptr);
            if (rc >= 0)
              found = true;
          } else {
            std::string lineSearch = line;
            if (!caseSensitive) {
              std::transform(lineSearch.begin(), lineSearch.end(),
                             lineSearch.begin(), ::tolower);
            }
            if (lineSearch.find(query) != std::string::npos)
              found = true;
          }

          if (found) {
            fileMatches.push_back({entry.path().string(), lineNum, line});
          }
        }

        // Batch update results
        if (!fileMatches.empty()) {
          std::lock_guard<std::mutex> lock(m_mutex);
          m_currentResult.matches.insert(m_currentResult.matches.end(),
                                         fileMatches.begin(),
                                         fileMatches.end());
        }
      }
    }
  } catch (const fs::filesystem_error &e) {
    // Permission denied etc.
    (void)e;
  }

  if (re)
    pcre2_code_free(re);
  if (match_data)
    pcre2_match_data_free(match_data);

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentResult.complete = true;
  }
}

} // namespace arcanee::ide
