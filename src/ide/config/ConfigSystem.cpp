// ARCANEE Config System - Implementation
// Copyright (C) 2025 Michele Fabbri - AGPL-3.0

#include "ConfigSystem.h"
#include "ConfigSchema.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <thread>

namespace fs = std::filesystem;

namespace arcanee::ide::config {

// -----------------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------------
ConfigSystem::ConfigSystem(const ConfigSystemInit &init) : m_init(init) {}

ConfigSystem::~ConfigSystem() {
  m_watcherRunning = false;
  if (m_watcherThread.joinable()) {
    m_watcherThread.join();
  }
}

// -----------------------------------------------------------------------------
// Initialize - discover config root and load all configs [REQ-90]
// -----------------------------------------------------------------------------
void ConfigSystem::Initialize() {
  spdlog::info("[ConfigSystem] Initialized");

  // Start watcher thread [REQ-HOTRELOAD]
  m_watcherRunning = true;
  m_watcherThread = std::thread(&ConfigSystem::WatcherLoop, this);

  DiscoverConfigRoot();
  LoadAllConfigs();
}

// -----------------------------------------------------------------------------
// Discover config root [DEC-67]
// Primary: ./config/ relative to cwd
// Fallback: exe_dir/config/ with warning
// -----------------------------------------------------------------------------
void ConfigSystem::DiscoverConfigRoot() {
  // Try cwd/config first
  fs::path cwd_config = fs::current_path() / "config";
  if (fs::exists(cwd_config) && fs::is_directory(cwd_config)) {
    m_configRoot = cwd_config.string();
    spdlog::info("[ConfigSystem] Using config root: {}", m_configRoot);
    return;
  }

  // Try to create it if it doesn't exist
  std::error_code ec;
  if (fs::create_directories(cwd_config, ec)) {
    m_configRoot = cwd_config.string();
    spdlog::info("[ConfigSystem] Created config root: {}", m_configRoot);
    return;
  }

  // Fallback: exe directory (would need platform-specific code)
  // For now, just use cwd/config and create it
  m_configRoot = cwd_config.string();
  spdlog::warn("[ConfigSystem] Config directory may not exist: {}",
               m_configRoot);
}

// -----------------------------------------------------------------------------
// Load all config files and build initial snapshot
// -----------------------------------------------------------------------------
void ConfigSystem::LoadAllConfigs() {
  auto snapshot = std::make_shared<ConfigSnapshot>();
  snapshot->paths.config_root = m_configRoot;

  ConfigSchema schema;

  // Collect diagnostics per file
  std::unordered_map<std::string, std::vector<ConfigDiagnostic>> allDiags;

  schema.SetDiagnosticCallback([&](const ConfigDiagnostic &diag) {
    allDiags[diag.file].push_back(diag);
  });

  // 1. Parse color-schemes.toml [REQ-90]
  std::string schemesPath = m_configRoot + "/color-schemes.toml";
  std::string schemesContent;
  if (ReadFileContent(schemesPath, schemesContent)) {
    schema.ParseColorSchemes(schemesContent, "config/color-schemes.toml",
                             snapshot->registry);
  } else {
    spdlog::warn(
        "[ConfigSystem] color-schemes.toml not found, using built-in defaults");
    // Add built-in Ayu Mirage scheme
    Scheme ayu;
    ayu.id = "ayu-mirage";
    ayu.name = "Ayu Mirage";
    ayu.variant = "dark";
    // Default palette values are already set in EditorPalette/SyntaxPalette
    // constructors
    ayu.syntax.token_rgba.resize(static_cast<size_t>(SyntaxToken::Count));
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::Comment)] =
        0x707A8CFF;
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::String)] =
        0xBAE67EFF;
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::Number)] =
        0xD4BFFFFF;
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::Keyword)] =
        0xFFAD66FF;
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::Type)] = 0x73D0FFFF;
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::Function)] =
        0xFFD173FF;
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::Variable)] =
        0xCBCCC6FF;
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::Operator)] =
        0xF29E74FF;
    ayu.syntax.token_rgba[static_cast<size_t>(SyntaxToken::Error)] = 0xFF6666FF;
    snapshot->registry.schemes_by_id["ayu-mirage"] = std::move(ayu);
  }

  // 2. Parse editor.toml (optional)
  std::string editorPath = m_configRoot + "/editor.toml";
  std::string editorContent;
  if (ReadFileContent(editorPath, editorContent)) {
    schema.ParseEditorConfig(editorContent, "config/editor.toml",
                             snapshot->editor, snapshot->active_scheme_id);
  }

  // 3. Parse gui.toml (optional)
  std::string guiPath = m_configRoot + "/gui.toml";
  std::string guiContent;
  if (ReadFileContent(guiPath, guiContent)) {
    schema.ParseGuiConfig(guiContent, "config/gui.toml", snapshot->gui);
  }

  // 4. Parse keys.toml (optional)
  std::string keysPath = m_configRoot + "/keys.toml";
  std::string keysContent;
  if (ReadFileContent(keysPath, keysContent)) {
    schema.ParseKeysConfig(keysContent, "config/keys.toml", snapshot->keys);
  }

  // Validate active scheme exists
  if (!snapshot->registry.Find(snapshot->active_scheme_id)) {
    spdlog::warn("[ConfigSystem] Active scheme '{}' not found, falling back to "
                 "first available",
                 snapshot->active_scheme_id);
    if (!snapshot->registry.schemes_by_id.empty()) {
      snapshot->active_scheme_id =
          snapshot->registry.schemes_by_id.begin()->first;
    }
  }

  // Set version
  snapshot->version = 1;

  // Store as current and last-good
  {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    m_current = snapshot;
    m_lastGood = snapshot;
  }

  // Publish diagnostics
  if (m_init.problems) {
    for (const auto &[file, diags] : allDiags) {
      m_init.problems->ReplaceDiagnosticsForFile(file, diags);
    }
  }

  // Invoke apply callback
  if (m_init.apply_on_main) {
    m_init.apply_on_main(snapshot);
  }

  spdlog::info("[ConfigSystem] Initial config loaded, active scheme: {}",
               snapshot->active_scheme_id);
}

// -----------------------------------------------------------------------------
// OnIdeSavedFile - called when IDE saves a file [REQ-91]
// -----------------------------------------------------------------------------
void ConfigSystem::OnIdeSavedFile(const std::string &absolute_path) {
  // Check if it's a config file
  if (absolute_path.find("/config/") == std::string::npos &&
      absolute_path.find("\\config\\") == std::string::npos) {
    return;
  }

  // Check for .toml extension
  if (absolute_path.size() < 5 ||
      absolute_path.substr(absolute_path.size() - 5) != ".toml") {
    return;
  }

  spdlog::debug("[ConfigSystem] Config file saved: {}", absolute_path);

  // Trigger debounced reload
  DebouncedReload();
}

// -----------------------------------------------------------------------------
// DebouncedReload - schedule reload after debounce interval [CONC-01]
// -----------------------------------------------------------------------------
void ConfigSystem::DebouncedReload() {
  uint64_t seq = ++m_reloadSeq;

  // Only schedule one debounce at a time
  bool expected = false;
  if (!m_debounceScheduled.compare_exchange_strong(expected, true)) {
    return;
  }

  if (m_init.post_to_worker) {
    m_init.post_to_worker([this, seq]() {
      // Wait for debounce
      std::this_thread::sleep_for(std::chrono::milliseconds(kDebounceMs));
      m_debounceScheduled = false;

      // Always perform reload for the LATEST sequence at the end of the wait
      // This ensures we catch any updates that happened while sleeping
      PerformReload(m_reloadSeq.load());
    });
  } else {
    // Synchronous fallback
    m_debounceScheduled = false;
    PerformReload(seq);
  }
}

// -----------------------------------------------------------------------------
// PerformReload - actually reload configs (worker thread)
// -----------------------------------------------------------------------------
void ConfigSystem::PerformReload(uint64_t reload_seq) {
  spdlog::debug("[ConfigSystem] Performing reload, seq={}", reload_seq);

  auto snapshot = std::make_shared<ConfigSnapshot>();
  snapshot->paths.config_root = m_configRoot;

  ConfigSchema schema;
  std::unordered_map<std::string, std::vector<ConfigDiagnostic>> allDiags;

  schema.SetDiagnosticCallback([&](const ConfigDiagnostic &diag) {
    allDiags[diag.file].push_back(diag);
  });

  bool anyError = false;

  // Load color-schemes.toml
  std::string schemesPath = m_configRoot + "/color-schemes.toml";
  std::string schemesContent;
  if (ReadFileContent(schemesPath, schemesContent)) {
    if (!schema.ParseColorSchemes(schemesContent, "config/color-schemes.toml",
                                  snapshot->registry)) {
      anyError = true;
    }
  }

  // Load editor.toml
  std::string editorPath = m_configRoot + "/editor.toml";
  std::string editorContent;
  if (ReadFileContent(editorPath, editorContent)) {
    if (!schema.ParseEditorConfig(editorContent, "config/editor.toml",
                                  snapshot->editor,
                                  snapshot->active_scheme_id)) {
      anyError = true;
    }
  }

  // Load gui.toml
  std::string guiPath = m_configRoot + "/gui.toml";
  std::string guiContent;
  if (ReadFileContent(guiPath, guiContent)) {
    if (!schema.ParseGuiConfig(guiContent, "config/gui.toml", snapshot->gui)) {
      anyError = true;
    }
  }

  // Load keys.toml
  std::string keysPath = m_configRoot + "/keys.toml";
  std::string keysContent;
  if (ReadFileContent(keysPath, keysContent)) {
    if (!schema.ParseKeysConfig(keysContent, "config/keys.toml",
                                snapshot->keys)) {
      anyError = true;
    }
  }

  // If fatal parse error, keep last-known-good [REQ-91]
  if (anyError && snapshot->registry.schemes_by_id.empty()) {
    spdlog::warn("[ConfigSystem] Parse errors, keeping last-known-good config");
    // Still publish diagnostics
    if (m_init.post_to_main) {
      m_init.post_to_main([this, allDiags, reload_seq]() {
        for (const auto &[file, diags] : allDiags) {
          PublishDiagnostics(file, diags, reload_seq);
        }
      });
    }
    return;
  }

  // Validate active scheme
  if (snapshot->active_scheme_id.empty()) {
    snapshot->active_scheme_id = "ayu-mirage";
  }
  if (!snapshot->registry.Find(snapshot->active_scheme_id)) {
    if (!snapshot->registry.schemes_by_id.empty()) {
      snapshot->active_scheme_id =
          snapshot->registry.schemes_by_id.begin()->first;
    }
  }

  // Update version
  {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    snapshot->version = m_current ? m_current->version + 1 : 1;
  }

  // Publish to main thread
  if (m_init.post_to_main) {
    m_init.post_to_main([this, snapshot, allDiags, reload_seq]() {
      PublishSnapshot(snapshot, reload_seq);
      for (const auto &[file, diags] : allDiags) {
        PublishDiagnostics(file, diags, reload_seq);
      }
    });
  } else {
    PublishSnapshot(snapshot, reload_seq);
  }
}

// -----------------------------------------------------------------------------
// PublishSnapshot - apply snapshot on main thread
// -----------------------------------------------------------------------------
void ConfigSystem::PublishSnapshot(ConfigSnapshotPtr snapshot, uint64_t seq) {
  // Check if this is still the latest [CONC-01]
  if (seq < m_latestAppliedSeq.load()) {
    spdlog::debug("[ConfigSystem] Discarding stale snapshot seq={}", seq);
    return;
  }
  m_latestAppliedSeq.store(seq);

  {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    m_current = snapshot;
    m_lastGood = snapshot;
  }

  if (m_init.apply_on_main) {
    m_init.apply_on_main(snapshot);
  }

  spdlog::info("[ConfigSystem] Config applied, version={}, scheme={}",
               snapshot->version, snapshot->active_scheme_id);
}

// -----------------------------------------------------------------------------
// PublishDiagnostics
// -----------------------------------------------------------------------------
void ConfigSystem::PublishDiagnostics(
    const std::string &file, const std::vector<ConfigDiagnostic> &diags,
    uint64_t seq) {
  if (seq < m_latestAppliedSeq.load()) {
    return; // Stale
  }
  if (m_init.problems) {
    m_init.problems->ReplaceDiagnosticsForFile(file, diags);
  }
}

// -----------------------------------------------------------------------------
// Current - get current snapshot (main thread only)
// -----------------------------------------------------------------------------
ConfigSnapshotPtr ConfigSystem::Current() const {
  std::lock_guard<std::mutex> lock(m_snapshotMutex);
  return m_current;
}

// -----------------------------------------------------------------------------
// ForceReload - for testing
// -----------------------------------------------------------------------------
void ConfigSystem::ForceReload() {
  uint64_t seq = ++m_reloadSeq;
  PerformReload(seq);
}

// -----------------------------------------------------------------------------
// ReadFileContent
// -----------------------------------------------------------------------------
bool ConfigSystem::ReadFileContent(const std::string &path,
                                   std::string &out_content) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);

  out_content.resize(size);
  file.read(out_content.data(), size);

  // UTF-8 validation [REQ-98]
  // Basic check: ensure no invalid byte sequences
  for (size_t i = 0; i < out_content.size(); ++i) {
    unsigned char c = out_content[i];
    if (c >= 0x80) {
      // Multi-byte UTF-8 sequence
      int expected = 0;
      if ((c & 0xE0) == 0xC0)
        expected = 1;
      else if ((c & 0xF0) == 0xE0)
        expected = 2;
      else if ((c & 0xF8) == 0xF0)
        expected = 3;
      else {
        spdlog::warn("[ConfigSystem] Invalid UTF-8 in {}", path);
        // Continue anyway, toml++ will handle it
      }
      // Skip continuation bytes (basic validation)
      i += expected;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// Watcher Loop - Poll for file changes [REQ-HOTRELOAD]
// -----------------------------------------------------------------------------
void ConfigSystem::WatcherLoop() {
  while (m_watcherRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::vector<std::string> changedFiles;
    {
      std::lock_guard<std::mutex> lock(m_watcherMutex);
      for (auto &[path, lastTime] : m_fileTimestamps) {
        std::error_code ec;
        auto currentTime = fs::last_write_time(path, ec);
        if (!ec && currentTime > lastTime) {
          lastTime = currentTime;
          changedFiles.push_back(path);
        }
      }
    }

    if (!changedFiles.empty()) {
      spdlog::info("[ConfigSystem] Detected external changes in {} files",
                   changedFiles.size());
      DebouncedReload();
    }
  }
}

void ConfigSystem::UpdateWatchedFiles(const std::vector<std::string> &files) {
  std::lock_guard<std::mutex> lock(m_watcherMutex);
  m_fileTimestamps.clear();
  for (const auto &file : files) {
    std::error_code ec;
    auto time = fs::last_write_time(file, ec);
    if (!ec) {
      m_fileTimestamps[file] = time;
    }
  }
}

} // namespace arcanee::ide::config
