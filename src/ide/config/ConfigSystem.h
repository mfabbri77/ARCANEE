// ARCANEE Config System - [API-19]
// Central coordinator for config load/merge/validate + hot-apply on IDE save.
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#pragma once
#include "ConfigSnapshot.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace arcanee::ide::config {

// -----------------------------------------------------------------------------
// ProblemsSink interface for diagnostics emission [API-18]
// -----------------------------------------------------------------------------
class ProblemsSink {
public:
  virtual ~ProblemsSink() = default;
  virtual void
  ReplaceDiagnosticsForFile(const std::string &file,
                            const std::vector<ConfigDiagnostic> &diags) = 0;
};

// -----------------------------------------------------------------------------
// ConfigSystemInit - initialization parameters
// -----------------------------------------------------------------------------
struct ConfigSystemInit {
  ProblemsSink *problems = nullptr;                     // Non-owning
  std::function<void(ConfigSnapshotPtr)> apply_on_main; // Called on main thread
  std::function<void(std::function<void()>)> post_to_main; // Enqueue to main/UI
  std::function<void(std::function<void()>)>
      post_to_worker; // Enqueue to worker
};

// -----------------------------------------------------------------------------
// ConfigSystem - central config coordinator [ARCH-12]
// -----------------------------------------------------------------------------
class ConfigSystem {
public:
  explicit ConfigSystem(const ConfigSystemInit &init);
  ~ConfigSystem();

  // Called once at startup; loads defaults + config files [REQ-90]
  void Initialize();

  // Called by DocumentSystem when IDE saves a file [REQ-91]
  void OnIdeSavedFile(const std::string &absolute_path);

  // Access last published snapshot (thread: main only)
  ConfigSnapshotPtr Current() const;

  // Get config root path
  const std::string &GetConfigRoot() const { return m_configRoot; }

  // Force reload (for testing or explicit refresh)
  void ForceReload();

private:
  void DiscoverConfigRoot();
  void LoadAllConfigs();
  void DebouncedReload();
  void PerformReload(uint64_t reload_seq);
  bool ReadFileContent(const std::string &path, std::string &out_content);
  void PublishSnapshot(ConfigSnapshotPtr snapshot, uint64_t seq);
  void PublishDiagnostics(const std::string &file,
                          const std::vector<ConfigDiagnostic> &diags,
                          uint64_t seq);

  // Initialization params
  ConfigSystemInit m_init;

  // Config root path
  std::string m_configRoot;

  // Current snapshot (main-thread access only)
  mutable std::mutex m_snapshotMutex;
  ConfigSnapshotPtr m_current;
  ConfigSnapshotPtr m_lastGood;

  // Reload sequencing [CONC-01]
  std::atomic<uint64_t> m_reloadSeq{0};
  std::atomic<uint64_t> m_latestAppliedSeq{0};

  // Debounce
  static constexpr int kDebounceMs = 200;
  std::atomic<bool> m_debounceScheduled{false};

  // File Watching [REQ-HOTRELOAD]
  std::thread m_watcherThread;
  std::atomic<bool> m_watcherRunning{false};
  std::mutex m_watcherMutex;
  std::map<std::string, std::filesystem::file_time_type> m_fileTimestamps;

  void WatcherLoop();
  void UpdateWatchedFiles(const std::vector<std::string> &files);

  // Diagnostics per file
  std::mutex m_diagMutex;
  std::unordered_map<std::string, std::vector<ConfigDiagnostic>> m_pendingDiags;
};

} // namespace arcanee::ide::config
