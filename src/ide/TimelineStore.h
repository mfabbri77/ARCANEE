#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct sqlite3;

namespace arcanee::ide {

struct HistoryEntry {
  int64_t id;
  std::string filePath;
  int64_t timestamp;   // Unix epoch seconds
  std::string trigger; // "save", "run", "timer", "manual"
  uint64_t hash;       // xxHash of content
  size_t originalSize;
  size_t compressedSize;
};

class TimelineStore {
public:
  TimelineStore();
  ~TimelineStore();

  // Initialize database at path
  bool Initialize(const std::string &dbPath);

  // Close and cleanup
  void Shutdown();

  // Save a snapshot of file content
  bool SaveSnapshot(const std::string &filePath, const std::string &content,
                    const std::string &trigger);

  // Get history entries for a file
  std::vector<HistoryEntry> GetHistory(const std::string &filePath,
                                       int limit = 50);

  // Restore content from a snapshot
  std::string RestoreSnapshot(int64_t id);

  // Prune entries older than days
  void PruneOld(int days);

  // Check if initialized
  bool IsInitialized() const { return m_db != nullptr; }

private:
  bool CreateTables();

  sqlite3 *m_db = nullptr;
  std::mutex m_mutex;
};

} // namespace arcanee::ide
