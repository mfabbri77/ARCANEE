#include "TimelineStore.h"
#include <ctime>
#include <sqlite3.h>
#include <xxhash.h>
#include <zstd.h>

namespace arcanee::ide {

TimelineStore::TimelineStore() {}

TimelineStore::~TimelineStore() { Shutdown(); }

bool TimelineStore::Initialize(const std::string &dbPath) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_db) {
    sqlite3_close(m_db);
    m_db = nullptr;
  }

  int rc = sqlite3_open(dbPath.c_str(), &m_db);
  if (rc != SQLITE_OK) {
    m_db = nullptr;
    return false;
  }

  return CreateTables();
}

void TimelineStore::Shutdown() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_db) {
    sqlite3_close(m_db);
    m_db = nullptr;
  }
}

bool TimelineStore::CreateTables() {
  const char *sql = R"(
        CREATE TABLE IF NOT EXISTS snapshots (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            filePath TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            trigger TEXT NOT NULL,
            hash INTEGER NOT NULL,
            originalSize INTEGER NOT NULL,
            compressedSize INTEGER NOT NULL,
            data BLOB NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_snapshots_path ON snapshots(filePath);
        CREATE INDEX IF NOT EXISTS idx_snapshots_time ON snapshots(timestamp);
    )";

  char *errMsg = nullptr;
  int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    if (errMsg)
      sqlite3_free(errMsg);
    return false;
  }
  return true;
}

bool TimelineStore::SaveSnapshot(const std::string &filePath,
                                 const std::string &content,
                                 const std::string &trigger) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_db)
    return false;

  // Compute hash
  uint64_t hash = XXH64(content.data(), content.size(), 0);

  // Check if identical content already exists (deduplication)
  const char *checkSql = "SELECT id FROM snapshots WHERE filePath = ? AND hash "
                         "= ? ORDER BY timestamp DESC LIMIT 1";
  sqlite3_stmt *checkStmt = nullptr;
  sqlite3_prepare_v2(m_db, checkSql, -1, &checkStmt, nullptr);
  sqlite3_bind_text(checkStmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(checkStmt, 2, (int64_t)hash);

  if (sqlite3_step(checkStmt) == SQLITE_ROW) {
    // Identical content already saved
    sqlite3_finalize(checkStmt);
    return true; // Skip saving duplicate
  }
  sqlite3_finalize(checkStmt);

  // Compress content
  size_t compBound = ZSTD_compressBound(content.size());
  std::vector<char> compressed(compBound);
  size_t compSize = ZSTD_compress(compressed.data(), compBound, content.data(),
                                  content.size(), 3);

  if (ZSTD_isError(compSize)) {
    return false;
  }

  // Insert into database
  const char *insertSql =
      "INSERT INTO snapshots (filePath, timestamp, trigger, hash, "
      "originalSize, compressedSize, data) VALUES (?, ?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt = nullptr;

  int rc = sqlite3_prepare_v2(m_db, insertSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK)
    return false;

  int64_t timestamp = (int64_t)std::time(nullptr);

  sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, timestamp);
  sqlite3_bind_text(stmt, 3, trigger.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, (int64_t)hash);
  sqlite3_bind_int64(stmt, 5, (int64_t)content.size());
  sqlite3_bind_int64(stmt, 6, (int64_t)compSize);
  sqlite3_bind_blob(stmt, 7, compressed.data(), (int)compSize,
                    SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE;
}

std::vector<HistoryEntry> TimelineStore::GetHistory(const std::string &filePath,
                                                    int limit) {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::vector<HistoryEntry> entries;

  if (!m_db)
    return entries;

  const char *sql = "SELECT id, filePath, timestamp, trigger, hash, "
                    "originalSize, compressedSize FROM snapshots WHERE "
                    "filePath = ? ORDER BY timestamp DESC LIMIT ?";
  sqlite3_stmt *stmt = nullptr;

  int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK)
    return entries;

  sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, limit);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    HistoryEntry entry;
    entry.id = sqlite3_column_int64(stmt, 0);
    entry.filePath = (const char *)sqlite3_column_text(stmt, 1);
    entry.timestamp = sqlite3_column_int64(stmt, 2);
    entry.trigger = (const char *)sqlite3_column_text(stmt, 3);
    entry.hash = (uint64_t)sqlite3_column_int64(stmt, 4);
    entry.originalSize = (size_t)sqlite3_column_int64(stmt, 5);
    entry.compressedSize = (size_t)sqlite3_column_int64(stmt, 6);
    entries.push_back(entry);
  }

  sqlite3_finalize(stmt);
  return entries;
}

std::string TimelineStore::RestoreSnapshot(int64_t id) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_db)
    return "";

  const char *sql = "SELECT data, originalSize FROM snapshots WHERE id = ?";
  sqlite3_stmt *stmt = nullptr;

  int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK)
    return "";

  sqlite3_bind_int64(stmt, 1, id);

  std::string result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const void *data = sqlite3_column_blob(stmt, 0);
    int compSize = sqlite3_column_bytes(stmt, 0);
    size_t origSize = (size_t)sqlite3_column_int64(stmt, 1);

    result.resize(origSize);
    size_t decompSize =
        ZSTD_decompress(result.data(), origSize, data, compSize);

    if (ZSTD_isError(decompSize)) {
      result.clear();
    }
  }

  sqlite3_finalize(stmt);
  return result;
}

void TimelineStore::PruneOld(int days) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_db)
    return;

  int64_t cutoff = (int64_t)std::time(nullptr) - (days * 24 * 60 * 60);

  const char *sql = "DELETE FROM snapshots WHERE timestamp < ?";
  sqlite3_stmt *stmt = nullptr;

  int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK)
    return;

  sqlite3_bind_int64(stmt, 1, cutoff);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

} // namespace arcanee::ide
