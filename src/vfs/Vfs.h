#pragma once

#include "common/Types.h"
#include <optional>
#include <string>
#include <vector>

namespace arcanee::vfs {

// VFS Namespace types (Chapter 3 §3.3.3)
enum class Namespace { Cart, Save, Temp, Invalid };

// Parse result for VFS paths
struct ParsedPath {
  Namespace ns;
  std::string relativePath; // Path after "ns:/"
};

// File stat info (Chapter 3 §3.7.5)
struct FileStat {
  enum class Type { File, Directory };
  Type type;
  u64 size;                 // 0 for directories
  std::optional<i64> mtime; // Unix epoch seconds, nullopt if unavailable
};

// VFS Error types
enum class VfsError {
  None,
  InvalidPath,
  PathTraversal, // ".." detected
  InvalidNamespace,
  FileNotFound,
  DirectoryNotFound,
  PermissionDenied,
  QuotaExceeded,
  IoError,
  NotInitialized
};

// Convert error to string for logging
const char *vfsErrorToString(VfsError err);

// Path utilities (Chapter 3 §3.6)
class Path {
public:
  // Normalize and validate a VFS path
  // Returns nullopt if path is invalid (contains "..", missing namespace, etc.)
  static std::optional<ParsedPath> parse(const std::string &input);

  // Get namespace from string ("cart", "save", "temp")
  static Namespace parseNamespace(const std::string &ns);

  // Convert namespace back to string
  static const char *namespaceToString(Namespace ns);

  // Join path segments
  static std::string join(const std::string &base, const std::string &relative);
};

// VFS Interface (Chapter 3, Appendix D §D.3.2)
class IVfs {
public:
  virtual ~IVfs() = default;

  // Initialize VFS with cartridge and user data paths
  virtual bool init(const std::string &cartridgePath,
                    const std::string &saveRootPath,
                    const std::string &tempRootPath) = 0;

  // Shutdown and unmount all filesystems
  virtual void shutdown() = 0;

  // File existence check
  virtual bool exists(const std::string &vfsPath) = 0;

  // Get file/directory info
  virtual std::optional<FileStat> stat(const std::string &vfsPath) = 0;

  // Read entire file as bytes
  virtual std::optional<std::vector<u8>>
  readBytes(const std::string &vfsPath) = 0;

  // Read entire file as UTF-8 text
  virtual std::optional<std::string> readText(const std::string &vfsPath) = 0;

  // Write bytes to file (save:/ or temp:/ only)
  virtual VfsError writeBytes(const std::string &vfsPath,
                              const std::vector<u8> &data) = 0;

  // Write UTF-8 text to file (save:/ or temp:/ only)
  virtual VfsError writeText(const std::string &vfsPath,
                             const std::string &text) = 0;

  // List directory contents (sorted lexicographically per §3.7.4)
  virtual std::optional<std::vector<std::string>>
  listDir(const std::string &vfsPath) = 0;

  // Create directory (save:/ or temp:/ only)
  virtual VfsError mkdir(const std::string &vfsPath) = 0;

  // Remove file or empty directory (save:/ or temp:/ only)
  virtual VfsError remove(const std::string &vfsPath) = 0;

  // Get last error for diagnostics
  virtual VfsError getLastError() const = 0;
  virtual std::string getLastErrorMessage() const = 0;
};

} // namespace arcanee::vfs
