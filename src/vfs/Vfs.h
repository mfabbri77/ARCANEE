#pragma once

/**
 * @file Vfs.h
 * @brief Virtual Filesystem interface and path utilities.
 *
 * Provides a sandboxed VFS with three namespaces:
 * - cart:/ : Read-only cartridge filesystem (directory or ZIP archive)
 * - save:/ : Writable persistent storage (per-cartridge)
 * - temp:/ : Writable temporary storage (per-cartridge, purgeable)
 *
 * @ref specs/Chapter 3 — Cartridge Format, VFS, Permissions, and Sandbox
 *      Complete VFS specification including path normalization, security,
 *      quotas, and API semantics.
 *
 * @ref specs/Appendix D §D.3.2
 *      "VFS / namespace → PhysFS mount management"
 */

#include "common/Types.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arcanee::vfs {

// ============================================================================
// VFS Namespace types (Chapter 3 §3.3.3)
// ============================================================================

/**
 * @brief VFS namespace identifiers.
 *
 * @ref specs/Chapter 3 §3.3.3
 *      "cart:/ — read from the cartridge's root"
 *      "save:/ — read/write persistent storage"
 *      "temp:/ — read/write ephemeral cache"
 */
enum class Namespace {
  Cart,   ///< Read-only cartridge filesystem
  Save,   ///< Writable persistent storage
  Temp,   ///< Writable temporary storage
  Invalid ///< Invalid/unknown namespace
};

// ============================================================================
// Parse result for VFS paths
// ============================================================================

struct ParsedPath {
  Namespace ns;             ///< The namespace of the path
  std::string relativePath; ///< Path after "ns:/"
};

// ============================================================================
// File stat info (Chapter 3 §3.7.5)
// ============================================================================

/**
 * @brief File/directory metadata.
 *
 * @ref specs/Chapter 3 §3.7.5
 *      "fs.stat(path) MUST return: type, size, mtime"
 */
struct FileStat {
  enum class Type { File, Directory };
  Type type;
  u64 size;                 ///< File size in bytes (0 for directories)
  std::optional<i64> mtime; ///< Unix epoch seconds, nullopt if unavailable
};

// ============================================================================
// VFS Error types
// ============================================================================

/**
 * @brief VFS operation error codes.
 *
 * @ref specs/Chapter 3 §3.8
 *      Error conditions for VFS operations.
 */
enum class VfsError {
  None,              ///< No error
  InvalidPath,       ///< Path is malformed
  PathTraversal,     ///< Path contains ".." (security violation)
  InvalidNamespace,  ///< Path has unknown or missing namespace
  FileNotFound,      ///< File does not exist
  DirectoryNotFound, ///< Directory does not exist
  NotAFile,          ///< Expected file but found directory
  NotADirectory,     ///< Expected directory but found file
  PermissionDenied,  ///< Write to read-only namespace or disabled permission
  QuotaExceeded,     ///< Storage quota exceeded
  IoError,           ///< General I/O error
  NotInitialized,    ///< VFS not initialized
  InvalidUtf8        ///< File is not valid UTF-8 (for readText)
};

/**
 * @brief Convert VFS error to human-readable string.
 */
const char *vfsErrorToString(VfsError err);

// ============================================================================
// Path utilities (Chapter 3 §3.6)
// ============================================================================

/**
 * @brief VFS path normalization and manipulation.
 *
 * @ref specs/Chapter 3 §3.6.1
 *      "ARCANEE MUST normalize VFS paths before resolution"
 */
class Path {
public:
  /**
   * @brief Parse and normalize a VFS path.
   *
   * Applies normalization rules per §3.6.1.1:
   * - Replace backslashes with forward slashes
   * - Collapse repeated separators
   * - Resolve "." segments
   * - REJECT paths containing ".." (security critical)
   *
   * @param input Raw input path (e.g., "cart:/scripts/main.nut")
   * @return Parsed path or nullopt if invalid/rejected.
   *
   * @ref specs/Chapter 3 §3.6.1.1
   *      "Reference Implementation (Normative Pseudocode)"
   */
  static std::optional<ParsedPath> parse(const std::string &input);

  /**
   * @brief Parse namespace string.
   *
   * @param ns Namespace string ("cart", "save", "temp")
   * @return Namespace enum or Invalid if unknown.
   */
  static Namespace parseNamespace(const std::string &ns);

  /**
   * @brief Convert namespace enum to string.
   */
  static const char *namespaceToString(Namespace ns);

  /**
   * @brief Join two path segments.
   *
   * Ensures single separator between parts.
   */
  static std::string join(const std::string &base, const std::string &relative);

  /**
   * @brief Get the parent directory of a path.
   *
   * @param path Path to get parent of (without namespace)
   * @return Parent path, or empty string if at root.
   */
  static std::string parent(const std::string &path);

  /**
   * @brief Get the filename from a path.
   *
   * @param path Path to get filename from (without namespace)
   * @return Filename (last segment).
   */
  static std::string filename(const std::string &path);

  /**
   * @brief Get the extension from a path.
   *
   * @param path Path or filename
   * @return Extension including dot (e.g., ".nut"), empty if none.
   */
  static std::string extension(const std::string &path);
};

// ============================================================================
// VFS Configuration
// ============================================================================

/**
 * @brief VFS initialization configuration.
 */
struct VfsConfig {
  std::string cartridgePath; ///< Path to cartridge (directory or ZIP)
  std::string cartridgeId;   ///< Cartridge ID for save/temp directories
  std::string saveRootPath;  ///< Base directory for save:/ mounts
  std::string tempRootPath;  ///< Base directory for temp:/ mounts
  bool saveEnabled = true;   ///< Allow writes to save:/
  u64 saveQuotaBytes = 50 * 1024 * 1024;  ///< 50 MB default
  u64 tempQuotaBytes = 100 * 1024 * 1024; ///< 100 MB default
};

// ============================================================================
// VFS Interface (Chapter 3, Appendix D §D.3.2)
// ============================================================================

/**
 * @brief Virtual Filesystem interface.
 *
 * All operations use VFS paths with namespace prefixes (cart:/, save:/,
 * temp:/). Paths are automatically normalized and validated.
 *
 * @ref specs/Chapter 3 §3.5
 *      "VFS Mount Rules (PhysFS-backed)"
 */
class IVfs {
public:
  virtual ~IVfs() = default;

  // --- Initialization ---

  /**
   * @brief Initialize the VFS with the given configuration.
   *
   * Sets up PhysFS and mounts the three namespaces.
   *
   * @param config VFS configuration.
   * @return true if successful.
   *
   * @ref specs/Chapter 3 §3.5.1
   *      "Mount Order and Search Path"
   */
  virtual bool init(const VfsConfig &config) = 0;

  /**
   * @brief Shutdown the VFS and unmount all filesystems.
   */
  virtual void shutdown() = 0;

  /**
   * @brief Check if VFS is initialized.
   */
  virtual bool isInitialized() const = 0;

  // --- File existence and metadata ---

  /**
   * @brief Check if a file or directory exists.
   *
   * @param vfsPath VFS path (e.g., "cart:/main.nut")
   * @return true if exists.
   */
  virtual bool exists(const std::string &vfsPath) = 0;

  /**
   * @brief Get file or directory metadata.
   *
   * @param vfsPath VFS path
   * @return FileStat or nullopt if not found/error.
   *
   * @ref specs/Chapter 3 §3.7.5
   *      "fs.stat(path) MUST return: type, size, mtime"
   */
  virtual std::optional<FileStat> stat(const std::string &vfsPath) = 0;

  // --- Reading ---

  /**
   * @brief Read file as raw bytes.
   *
   * @param vfsPath VFS path
   * @return File contents or nullopt on error.
   *
   * @ref specs/Chapter 3 §3.7.2
   *      "fs.readBytes() returns an array of ints in range [0,255]"
   */
  virtual std::optional<std::vector<u8>>
  readBytes(const std::string &vfsPath) = 0;

  /**
   * @brief Read file as UTF-8 text.
   *
   * @param vfsPath VFS path
   * @return File contents or nullopt on error (including invalid UTF-8).
   *
   * @ref specs/Chapter 3 §3.7.1
   *      "fs.readText() MUST return UTF-8 text"
   *      "If the file is not valid UTF-8, fs.readText() MUST fail"
   */
  virtual std::optional<std::string> readText(const std::string &vfsPath) = 0;

  // --- Writing (save:/ and temp:/ only) ---

  /**
   * @brief Write raw bytes to file.
   *
   * @param vfsPath VFS path (must be save:/ or temp:/)
   * @param data Data to write
   * @return Error code.
   *
   * @ref specs/Chapter 3 §3.5.2
   *      "Writes to cart:/ MUST fail"
   *
   * @ref specs/Chapter 3 §3.7.3
   *      "fs.writeBytes() writes exactly the provided bytes"
   */
  virtual VfsError writeBytes(const std::string &vfsPath,
                              const std::vector<u8> &data) = 0;

  /**
   * @brief Write UTF-8 text to file.
   *
   * @param vfsPath VFS path (must be save:/ or temp:/)
   * @param text Text to write
   * @return Error code.
   *
   * @ref specs/Chapter 3 §3.7.1
   *      "fs.writeText() MUST write UTF-8 text"
   */
  virtual VfsError writeText(const std::string &vfsPath,
                             const std::string &text) = 0;

  // --- Directory operations ---

  /**
   * @brief List directory contents.
   *
   * @param vfsPath VFS path to directory
   * @return List of entry names (not full paths), sorted lexicographically.
   *
   * @ref specs/Chapter 3 §3.7.4
   *      "fs.listDir() MUST return entries in stable lexicographical order"
   */
  virtual std::optional<std::vector<std::string>>
  listDir(const std::string &vfsPath) = 0;

  /**
   * @brief Create a directory.
   *
   * @param vfsPath VFS path (must be save:/ or temp:/)
   * @return Error code.
   */
  virtual VfsError mkdir(const std::string &vfsPath) = 0;

  /**
   * @brief Remove a file or empty directory.
   *
   * @param vfsPath VFS path (must be save:/ or temp:/)
   * @return Error code.
   */
  virtual VfsError remove(const std::string &vfsPath) = 0;

  // --- Diagnostics ---

  /**
   * @brief Get the last error that occurred.
   */
  virtual VfsError getLastError() const = 0;

  /**
   * @brief Get a detailed error message.
   */
  virtual std::string getLastErrorMessage() const = 0;

  // --- Quota management ---

  /**
   * @brief Get current storage usage for a namespace.
   *
   * @param ns Namespace to query (Save or Temp)
   * @return Bytes used, or 0 if not applicable.
   */
  virtual u64 getUsedBytes(Namespace ns) const = 0;

  /**
   * @brief Get quota limit for a namespace.
   *
   * @param ns Namespace to query (Save or Temp)
   * @return Quota in bytes.
   */
  virtual u64 getQuotaBytes(Namespace ns) const = 0;
};

/**
 * @brief Create the default PhysFS-backed VFS implementation.
 *
 * @return Unique pointer to VFS instance.
 */
std::unique_ptr<IVfs> createVfs();

} // namespace arcanee::vfs
