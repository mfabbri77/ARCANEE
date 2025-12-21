/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file VfsImpl.cpp
 * @brief PhysFS-backed VFS implementation.
 *
 * Implements the IVfs interface using PhysFS for portable filesystem access.
 * Handles mounting, namespace isolation, quota enforcement, and atomic writes.
 *
 * @ref specs/Chapter 3 §3.5
 *      "VFS Mount Rules (PhysFS-backed)"
 *
 * @ref specs/Appendix D §D.3.2
 *      "PhysFS init / mount with PHYSFS_mount, PHYSFS_setWriteDir"
 */

#include "Vfs.h"
#include "common/Log.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <physfs.h>
#include <sys/stat.h>

namespace arcanee::vfs {

namespace fs = std::filesystem;

// ============================================================================
// PhysFS VFS Implementation
// ============================================================================

/**
 * @brief PhysFS-backed implementation of IVfs.
 *
 * Uses a separate PhysFS context for read operations and native filesystem
 * operations for writes (since PhysFS can only have one write directory).
 *
 * Mount structure:
 * - cart:/ -> mounted at "/cart" in PhysFS search path
 * - save:/ and temp:/ -> handled via native filesystem at derived paths
 *
 * @ref specs/Chapter 3 §3.5.1
 *      "ARCANEE MUST mount resources in a deterministic order"
 */
class VfsImpl : public IVfs {
public:
  VfsImpl() = default;
  ~VfsImpl() override { shutdown(); }

  // ==== Initialization ====

  bool init(const VfsConfig &config) override {
    if (m_initialized) {
      LOG_WARN("VfsImpl::init: Already initialized");
      return true;
    }

    // Initialize PhysFS
    // @ref specs/Appendix D §D.3.2: "PhysFS init"
    if (!PHYSFS_init(nullptr)) {
      setError(VfsError::IoError,
               PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
      LOG_ERROR("VfsImpl: Failed to initialize PhysFS: %s",
                m_lastErrorMessage.c_str());
      return false;
    }

    m_config = config;

    // Mount cart:/ (read-only cartridge filesystem)
    // @ref specs/Chapter 3 §3.5.1: "cart:/ — cartridge source"
    if (!mountCartridge(config.cartridgePath)) {
      PHYSFS_deinit();
      return false;
    }

    // Setup save:/ and temp:/ directories
    // @ref specs/Chapter 3 §3.5.3: "Save and Temp Root Mapping"
    if (!setupWritableNamespaces(config)) {
      PHYSFS_deinit();
      return false;
    }

    m_initialized = true;
    LOG_INFO("VfsImpl: Initialized with cart='%s', id='%s'",
             config.cartridgePath.c_str(), config.cartridgeId.c_str());

    return true;
  }

  void shutdown() override {
    if (!m_initialized) {
      return;
    }

    PHYSFS_deinit();
    m_initialized = false;
    m_cartMounted = false;
    m_savePath.clear();
    m_tempPath.clear();

    LOG_INFO("VfsImpl: Shutdown complete");
  }

  bool isInitialized() const override { return m_initialized; }

  // ==== Existence and Metadata ====

  bool exists(const std::string &vfsPath) override {
    if (!checkInitialized()) {
      return false;
    }

    auto parsed = Path::parse(vfsPath);
    if (!parsed) {
      setError(VfsError::InvalidPath, "Invalid path: " + vfsPath);
      return false;
    }

    return existsInternal(parsed->ns, parsed->relativePath);
  }

  std::optional<FileStat> stat(const std::string &vfsPath) override {
    if (!checkInitialized()) {
      return std::nullopt;
    }

    auto parsed = Path::parse(vfsPath);
    if (!parsed) {
      setError(VfsError::InvalidPath, "Invalid path: " + vfsPath);
      return std::nullopt;
    }

    return statInternal(parsed->ns, parsed->relativePath);
  }

  // ==== Reading ====

  std::optional<std::vector<u8>>
  readBytes(const std::string &vfsPath) override {
    if (!checkInitialized()) {
      return std::nullopt;
    }

    auto parsed = Path::parse(vfsPath);
    if (!parsed) {
      setError(VfsError::InvalidPath, "Invalid path: " + vfsPath);
      return std::nullopt;
    }

    return readBytesInternal(parsed->ns, parsed->relativePath);
  }

  std::optional<std::string> readText(const std::string &vfsPath) override {
    auto bytes = readBytes(vfsPath);
    if (!bytes) {
      return std::nullopt;
    }

    // Validate UTF-8
    // @ref specs/Chapter 3 §3.7.1: "If the file is not valid UTF-8, fail"
    std::string text(bytes->begin(), bytes->end());
    if (!isValidUtf8(text)) {
      setError(VfsError::InvalidUtf8, "File is not valid UTF-8: " + vfsPath);
      return std::nullopt;
    }

    return text;
  }

  // ==== Writing ====

  VfsError writeBytes(const std::string &vfsPath,
                      const std::vector<u8> &data) override {
    if (!checkInitialized()) {
      return VfsError::NotInitialized;
    }

    auto parsed = Path::parse(vfsPath);
    if (!parsed) {
      setError(VfsError::InvalidPath, "Invalid path: " + vfsPath);
      return VfsError::InvalidPath;
    }

    // Writes to cart:/ are forbidden
    // @ref specs/Chapter 3 §3.5.2: "Writes to cart:/ MUST fail"
    if (parsed->ns == Namespace::Cart) {
      setError(VfsError::PermissionDenied, "Cannot write to cart:/ namespace");
      return VfsError::PermissionDenied;
    }

    // Check save:/ permission
    if (parsed->ns == Namespace::Save && !m_config.saveEnabled) {
      setError(VfsError::PermissionDenied,
               "save:/ writes disabled by permission");
      return VfsError::PermissionDenied;
    }

    // Check quota
    // @ref specs/Chapter 3 §3.8.1: "If quota is exceeded, writes MUST fail"
    u64 quota = getQuotaBytes(parsed->ns);
    u64 used = getUsedBytes(parsed->ns);
    if (used + data.size() > quota) {
      setError(VfsError::QuotaExceeded, "Storage quota exceeded");
      return VfsError::QuotaExceeded;
    }

    return writeBytesInternal(parsed->ns, parsed->relativePath, data);
  }

  VfsError writeText(const std::string &vfsPath,
                     const std::string &text) override {
    std::vector<u8> data(text.begin(), text.end());
    return writeBytes(vfsPath, data);
  }

  // ==== Directory Operations ====

  std::optional<std::vector<std::string>>
  listDir(const std::string &vfsPath) override {
    if (!checkInitialized()) {
      return std::nullopt;
    }

    auto parsed = Path::parse(vfsPath);
    if (!parsed) {
      setError(VfsError::InvalidPath, "Invalid path: " + vfsPath);
      return std::nullopt;
    }

    auto entries = listDirInternal(parsed->ns, parsed->relativePath);
    if (!entries) {
      return std::nullopt;
    }

    // Sort lexicographically for determinism
    // @ref specs/Chapter 3 §3.7.4: "stable lexicographical order"
    std::sort(entries->begin(), entries->end());
    return entries;
  }

  VfsError mkdir(const std::string &vfsPath) override {
    if (!checkInitialized()) {
      return VfsError::NotInitialized;
    }

    auto parsed = Path::parse(vfsPath);
    if (!parsed) {
      setError(VfsError::InvalidPath, "Invalid path: " + vfsPath);
      return VfsError::InvalidPath;
    }

    if (parsed->ns == Namespace::Cart) {
      setError(VfsError::PermissionDenied, "Cannot create directory in cart:/");
      return VfsError::PermissionDenied;
    }

    return mkdirInternal(parsed->ns, parsed->relativePath);
  }

  VfsError remove(const std::string &vfsPath) override {
    if (!checkInitialized()) {
      return VfsError::NotInitialized;
    }

    auto parsed = Path::parse(vfsPath);
    if (!parsed) {
      setError(VfsError::InvalidPath, "Invalid path: " + vfsPath);
      return VfsError::InvalidPath;
    }

    if (parsed->ns == Namespace::Cart) {
      setError(VfsError::PermissionDenied, "Cannot remove from cart:/");
      return VfsError::PermissionDenied;
    }

    return removeInternal(parsed->ns, parsed->relativePath);
  }

  // ==== Diagnostics ====

  VfsError getLastError() const override { return m_lastError; }

  std::string getLastErrorMessage() const override {
    return m_lastErrorMessage;
  }

  // ==== Quota Management ====

  u64 getUsedBytes(Namespace ns) const override {
    std::string basePath;
    switch (ns) {
    case Namespace::Save:
      basePath = m_savePath;
      break;
    case Namespace::Temp:
      basePath = m_tempPath;
      break;
    default:
      return 0;
    }

    if (basePath.empty() || !fs::exists(basePath)) {
      return 0;
    }

    u64 total = 0;
    std::error_code ec;
    for (const auto &entry : fs::recursive_directory_iterator(basePath, ec)) {
      if (entry.is_regular_file(ec)) {
        total += entry.file_size(ec);
      }
    }
    return total;
  }

  u64 getQuotaBytes(Namespace ns) const override {
    switch (ns) {
    case Namespace::Save:
      return m_config.saveQuotaBytes;
    case Namespace::Temp:
      return m_config.tempQuotaBytes;
    default:
      return 0;
    }
  }

private:
  // ==== Internal Helpers ====

  bool checkInitialized() {
    if (!m_initialized) {
      setError(VfsError::NotInitialized, "VFS not initialized");
      return false;
    }
    return true;
  }

  void setError(VfsError err, const std::string &msg = "") {
    m_lastError = err;
    m_lastErrorMessage = msg.empty() ? vfsErrorToString(err) : msg;
  }

  void clearError() {
    m_lastError = VfsError::None;
    m_lastErrorMessage.clear();
  }

  /**
   * @brief Mount the cartridge filesystem.
   *
   * @ref specs/Chapter 3 §3.5.1: "cart:/ — cartridge source (directory or
   * archive)"
   */
  bool mountCartridge(const std::string &path) {
    // Check if path exists
    if (!fs::exists(path)) {
      setError(VfsError::FileNotFound, "Cartridge path not found: " + path);
      LOG_ERROR("VfsImpl: Cartridge path not found: %s", path.c_str());
      return false;
    }

    // Mount the cartridge at the root of PhysFS search path
    // mountPoint "" means root of the virtual filesystem
    if (!PHYSFS_mount(path.c_str(), nullptr, 1)) {
      setError(VfsError::IoError,
               PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
      LOG_ERROR("VfsImpl: Failed to mount cartridge: %s",
                m_lastErrorMessage.c_str());
      return false;
    }

    m_cartMounted = true;
    LOG_DEBUG("VfsImpl: Mounted cart:/ from '%s'", path.c_str());
    return true;
  }

  /**
   * @brief Setup save:/ and temp:/ directories.
   *
   * @ref specs/Chapter 3 §3.5.3:
   *      "The runtime MUST derive a unique host directory based on
   *       the cartridge id"
   */
  bool setupWritableNamespaces(const VfsConfig &config) {
    // Derive paths based on cartridge ID
    // @ref specs/Chapter 3 §3.5.3: "Recommended host mapping"
    m_savePath = (fs::path(config.saveRootPath) / config.cartridgeId).string();
    m_tempPath = (fs::path(config.tempRootPath) / config.cartridgeId).string();

    std::error_code ec;

    // Create save directory if save is enabled
    if (config.saveEnabled) {
      if (!fs::exists(m_savePath)) {
        fs::create_directories(m_savePath, ec);
        if (ec) {
          setError(VfsError::IoError,
                   "Failed to create save directory: " + ec.message());
          LOG_ERROR("VfsImpl: Failed to create save directory: %s",
                    ec.message().c_str());
          return false;
        }
      }
      LOG_DEBUG("VfsImpl: save:/ at '%s'", m_savePath.c_str());
    }

    // Create temp directory
    if (!fs::exists(m_tempPath)) {
      fs::create_directories(m_tempPath, ec);
      if (ec) {
        setError(VfsError::IoError,
                 "Failed to create temp directory: " + ec.message());
        LOG_ERROR("VfsImpl: Failed to create temp directory: %s",
                  ec.message().c_str());
        return false;
      }
    }
    LOG_DEBUG("VfsImpl: temp:/ at '%s'", m_tempPath.c_str());

    return true;
  }

  /**
   * @brief Get host filesystem path for a writable namespace.
   */
  std::string getHostPath(Namespace ns, const std::string &relativePath) const {
    std::string basePath;
    switch (ns) {
    case Namespace::Save:
      basePath = m_savePath;
      break;
    case Namespace::Temp:
      basePath = m_tempPath;
      break;
    default:
      return "";
    }

    if (relativePath.empty()) {
      return basePath;
    }
    return (fs::path(basePath) / relativePath).string();
  }

  // ==== Namespace-specific operations ====

  bool existsInternal(Namespace ns, const std::string &relativePath) {
    if (ns == Namespace::Cart) {
      return PHYSFS_exists(relativePath.c_str()) != 0;
    } else {
      std::string hostPath = getHostPath(ns, relativePath);
      return fs::exists(hostPath);
    }
  }

  std::optional<FileStat> statInternal(Namespace ns,
                                       const std::string &relativePath) {
    FileStat stat{};

    if (ns == Namespace::Cart) {
      PHYSFS_Stat pstat;
      if (!PHYSFS_stat(relativePath.c_str(), &pstat)) {
        setError(VfsError::FileNotFound,
                 PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return std::nullopt;
      }

      stat.type = (pstat.filetype == PHYSFS_FILETYPE_DIRECTORY)
                      ? FileStat::Type::Directory
                      : FileStat::Type::File;
      stat.size = (pstat.filesize >= 0) ? static_cast<u64>(pstat.filesize) : 0;
      stat.mtime = (pstat.modtime >= 0) ? std::optional<i64>(pstat.modtime)
                                        : std::nullopt;
    } else {
      std::string hostPath = getHostPath(ns, relativePath);
      std::error_code ec;

      if (!fs::exists(hostPath, ec)) {
        setError(VfsError::FileNotFound, "Path not found: " + hostPath);
        return std::nullopt;
      }

      stat.type = fs::is_directory(hostPath, ec) ? FileStat::Type::Directory
                                                 : FileStat::Type::File;
      stat.size =
          fs::is_regular_file(hostPath, ec) ? fs::file_size(hostPath, ec) : 0;

      // Get mtime using POSIX stat (C++17 compatible)
      // NOTE: std::filesystem::file_time_type to time_t conversion
      // is not standardized until C++20, so we use stat() directly
      struct ::stat st;
      if (::stat(hostPath.c_str(), &st) == 0) {
        stat.mtime = static_cast<i64>(st.st_mtime);
      }
    }

    return stat;
  }

  std::optional<std::vector<u8>>
  readBytesInternal(Namespace ns, const std::string &relativePath) {
    if (ns == Namespace::Cart) {
      // Read from PhysFS
      PHYSFS_File *file = PHYSFS_openRead(relativePath.c_str());
      if (!file) {
        setError(VfsError::FileNotFound,
                 PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return std::nullopt;
      }

      PHYSFS_sint64 size = PHYSFS_fileLength(file);
      if (size < 0) {
        PHYSFS_close(file);
        setError(VfsError::IoError, "Could not determine file size");
        return std::nullopt;
      }

      std::vector<u8> data(static_cast<size_t>(size));
      PHYSFS_sint64 bytesRead = PHYSFS_readBytes(file, data.data(), size);
      PHYSFS_close(file);

      if (bytesRead != size) {
        setError(VfsError::IoError, "Incomplete read");
        return std::nullopt;
      }

      return data;
    } else {
      // Read from native filesystem
      std::string hostPath = getHostPath(ns, relativePath);
      std::error_code ec;

      if (!fs::exists(hostPath, ec) || !fs::is_regular_file(hostPath, ec)) {
        setError(VfsError::FileNotFound, "File not found: " + hostPath);
        return std::nullopt;
      }

      auto fileSize = fs::file_size(hostPath, ec);
      if (ec) {
        setError(VfsError::IoError, ec.message());
        return std::nullopt;
      }

      std::vector<u8> data(fileSize);
      FILE *fp = fopen(hostPath.c_str(), "rb");
      if (!fp) {
        setError(VfsError::IoError, "Could not open file");
        return std::nullopt;
      }

      size_t bytesRead = fread(data.data(), 1, fileSize, fp);
      fclose(fp);

      if (bytesRead != fileSize) {
        setError(VfsError::IoError, "Incomplete read");
        return std::nullopt;
      }

      return data;
    }
  }

  VfsError writeBytesInternal(Namespace ns, const std::string &relativePath,
                              const std::vector<u8> &data) {
    std::string hostPath = getHostPath(ns, relativePath);
    if (hostPath.empty()) {
      setError(VfsError::InvalidNamespace, "Invalid namespace for write");
      return VfsError::InvalidNamespace;
    }

    // Ensure parent directory exists
    std::string parentPath = Path::parent(relativePath);
    if (!parentPath.empty()) {
      std::string hostParent = getHostPath(ns, parentPath);
      std::error_code ec;
      fs::create_directories(hostParent, ec);
      if (ec) {
        setError(VfsError::IoError,
                 "Could not create parent directory: " + ec.message());
        return VfsError::IoError;
      }
    }

    // Atomic write: write to temp file then rename
    // @ref specs/Chapter 3 §3.7.3:
    //      "write to temp file, flush, rename/replace"
    std::string tempPath = hostPath + ".tmp";

    FILE *fp = fopen(tempPath.c_str(), "wb");
    if (!fp) {
      setError(VfsError::IoError, "Could not create file");
      return VfsError::IoError;
    }

    size_t written = fwrite(data.data(), 1, data.size(), fp);
    fflush(fp);
    fclose(fp);

    if (written != data.size()) {
      std::remove(tempPath.c_str());
      setError(VfsError::IoError, "Incomplete write");
      return VfsError::IoError;
    }

    // Rename temp to final
    std::error_code ec;
    fs::rename(tempPath, hostPath, ec);
    if (ec) {
      std::remove(tempPath.c_str());
      setError(VfsError::IoError,
               "Could not rename temp file: " + ec.message());
      return VfsError::IoError;
    }

    clearError();
    return VfsError::None;
  }

  std::optional<std::vector<std::string>>
  listDirInternal(Namespace ns, const std::string &relativePath) {
    std::vector<std::string> entries;

    if (ns == Namespace::Cart) {
      char **files = PHYSFS_enumerateFiles(relativePath.c_str());
      if (!files) {
        setError(VfsError::DirectoryNotFound,
                 PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return std::nullopt;
      }

      for (char **i = files; *i != nullptr; ++i) {
        entries.push_back(*i);
      }
      PHYSFS_freeList(files);
    } else {
      std::string hostPath = getHostPath(ns, relativePath);
      std::error_code ec;

      if (!fs::exists(hostPath, ec) || !fs::is_directory(hostPath, ec)) {
        setError(VfsError::DirectoryNotFound,
                 "Directory not found: " + hostPath);
        return std::nullopt;
      }

      for (const auto &entry : fs::directory_iterator(hostPath, ec)) {
        entries.push_back(entry.path().filename().string());
      }
    }

    return entries;
  }

  VfsError mkdirInternal(Namespace ns, const std::string &relativePath) {
    std::string hostPath = getHostPath(ns, relativePath);
    if (hostPath.empty()) {
      return VfsError::InvalidNamespace;
    }

    std::error_code ec;
    fs::create_directories(hostPath, ec);
    if (ec) {
      setError(VfsError::IoError, ec.message());
      return VfsError::IoError;
    }

    clearError();
    return VfsError::None;
  }

  VfsError removeInternal(Namespace ns, const std::string &relativePath) {
    std::string hostPath = getHostPath(ns, relativePath);
    if (hostPath.empty()) {
      return VfsError::InvalidNamespace;
    }

    std::error_code ec;
    if (!fs::exists(hostPath, ec)) {
      setError(VfsError::FileNotFound, "Path not found");
      return VfsError::FileNotFound;
    }

    fs::remove(hostPath, ec);
    if (ec) {
      setError(VfsError::IoError, ec.message());
      return VfsError::IoError;
    }

    clearError();
    return VfsError::None;
  }

  /**
   * @brief Simple UTF-8 validation.
   *
   * @ref specs/Chapter 3 §3.7.1:
   *      "If the file is not valid UTF-8, fs.readText() MUST fail"
   */
  static bool isValidUtf8(const std::string &str) {
    const unsigned char *bytes =
        reinterpret_cast<const unsigned char *>(str.c_str());
    const unsigned char *end = bytes + str.size();

    while (bytes < end) {
      if (*bytes <= 0x7F) {
        // ASCII
        bytes++;
      } else if ((*bytes & 0xE0) == 0xC0) {
        // 2-byte sequence
        if (end - bytes < 2 || (bytes[1] & 0xC0) != 0x80) {
          return false;
        }
        bytes += 2;
      } else if ((*bytes & 0xF0) == 0xE0) {
        // 3-byte sequence
        if (end - bytes < 3 || (bytes[1] & 0xC0) != 0x80 ||
            (bytes[2] & 0xC0) != 0x80) {
          return false;
        }
        bytes += 3;
      } else if ((*bytes & 0xF8) == 0xF0) {
        // 4-byte sequence
        if (end - bytes < 4 || (bytes[1] & 0xC0) != 0x80 ||
            (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) {
          return false;
        }
        bytes += 4;
      } else {
        return false;
      }
    }
    return true;
  }

  // ==== State ====

  bool m_initialized = false;
  bool m_cartMounted = false;
  VfsConfig m_config;
  std::string m_savePath;
  std::string m_tempPath;

  VfsError m_lastError = VfsError::None;
  std::string m_lastErrorMessage;
};

// ============================================================================
// Factory function
// ============================================================================

std::unique_ptr<IVfs> createVfs() { return std::make_unique<VfsImpl>(); }

} // namespace arcanee::vfs
