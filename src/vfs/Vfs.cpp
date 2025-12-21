/**
 * @file Vfs.cpp
 * @brief VFS path utilities and error handling implementation.
 *
 * Contains path normalization per the normative pseudocode in §3.6.1.1.
 * The actual PhysFS-backed filesystem operations are in VfsImpl.cpp.
 *
 * @ref specs/Chapter 3 §3.6.1
 *      "ARCANEE MUST normalize VFS paths before resolution"
 */

#include "Vfs.h"
#include "common/Log.h"
#include <algorithm>
#include <sstream>

namespace arcanee::vfs {

// ============================================================================
// Error string conversion
// ============================================================================

const char *vfsErrorToString(VfsError err) {
  switch (err) {
  case VfsError::None:
    return "No error";
  case VfsError::InvalidPath:
    return "Invalid path";
  case VfsError::PathTraversal:
    return "Path traversal (..) not allowed";
  case VfsError::InvalidNamespace:
    return "Invalid namespace (must be cart:/, save:/, or temp:/)";
  case VfsError::FileNotFound:
    return "File not found";
  case VfsError::DirectoryNotFound:
    return "Directory not found";
  case VfsError::NotAFile:
    return "Not a file";
  case VfsError::NotADirectory:
    return "Not a directory";
  case VfsError::PermissionDenied:
    return "Permission denied";
  case VfsError::QuotaExceeded:
    return "Storage quota exceeded";
  case VfsError::IoError:
    return "I/O error";
  case VfsError::NotInitialized:
    return "VFS not initialized";
  case VfsError::InvalidUtf8:
    return "File is not valid UTF-8";
  }
  return "Unknown error";
}

// ============================================================================
// Path Implementation - Based on Chapter 3 §3.6.1.1 Normative Pseudocode
// ============================================================================

std::optional<ParsedPath> Path::parse(const std::string &input) {
  // Step 1: Find namespace separator ":/"
  // @ref specs/Chapter 3 §3.6.1.1
  size_t colonPos = input.find(":/");
  if (colonPos == std::string::npos) {
    LOG_DEBUG("Path::parse: missing namespace separator in '%s'",
              input.c_str());
    return std::nullopt;
  }

  // Step 2: Extract and validate namespace
  std::string nsStr = input.substr(0, colonPos);
  Namespace ns = parseNamespace(nsStr);
  if (ns == Namespace::Invalid) {
    LOG_DEBUG("Path::parse: invalid namespace '%s'", nsStr.c_str());
    return std::nullopt;
  }

  // Step 3: Extract path after ":/"
  std::string path = input.substr(colonPos + 2);

  // Step 4: Replace backslashes with forward slashes
  // @ref specs/Chapter 3 §3.6.1: "Accept both / and \ in inputs"
  std::replace(path.begin(), path.end(), '\\', '/');

  // Step 5: Split into segments and process
  // @ref specs/Chapter 3 §3.6.1.1: Segment processing
  std::vector<std::string> segments;
  std::istringstream stream(path);
  std::string segment;

  while (std::getline(stream, segment, '/')) {
    // Skip empty segments (handles // -> /)
    if (segment.empty()) {
      continue;
    }

    // Skip current-dir segments
    // @ref specs/Chapter 3 §3.6.1: "Resolve . segments"
    if (segment == ".") {
      continue;
    }

    // REJECT parent traversal - SECURITY CRITICAL
    // @ref specs/Chapter 3 §3.6.1: "Reject any path containing .."
    if (segment == "..") {
      LOG_DEBUG("Path::parse: parent traversal (..) rejected in '%s'",
                input.c_str());
      return std::nullopt;
    }

    segments.push_back(segment);
  }

  // Step 6: Reconstruct normalized path
  std::string result;
  for (size_t i = 0; i < segments.size(); ++i) {
    result += segments[i];
    if (i < segments.size() - 1) {
      result += "/";
    }
  }

  return ParsedPath{ns, result};
}

Namespace Path::parseNamespace(const std::string &ns) {
  if (ns == "cart")
    return Namespace::Cart;
  if (ns == "save")
    return Namespace::Save;
  if (ns == "temp")
    return Namespace::Temp;
  return Namespace::Invalid;
}

const char *Path::namespaceToString(Namespace ns) {
  switch (ns) {
  case Namespace::Cart:
    return "cart";
  case Namespace::Save:
    return "save";
  case Namespace::Temp:
    return "temp";
  case Namespace::Invalid:
    return "invalid";
  }
  return "unknown";
}

std::string Path::join(const std::string &base, const std::string &relative) {
  if (base.empty())
    return relative;
  if (relative.empty())
    return base;

  // Ensure single separator between parts
  bool baseEndsSlash = base.back() == '/';
  bool relStartsSlash = relative.front() == '/';

  if (baseEndsSlash && relStartsSlash) {
    return base + relative.substr(1);
  } else if (!baseEndsSlash && !relStartsSlash) {
    return base + "/" + relative;
  }
  return base + relative;
}

std::string Path::parent(const std::string &path) {
  if (path.empty()) {
    return "";
  }

  // Find last separator
  size_t pos = path.rfind('/');
  if (pos == std::string::npos) {
    return ""; // No parent, at root
  }

  return path.substr(0, pos);
}

std::string Path::filename(const std::string &path) {
  if (path.empty()) {
    return "";
  }

  // Find last separator
  size_t pos = path.rfind('/');
  if (pos == std::string::npos) {
    return path; // No separator, path is filename
  }

  return path.substr(pos + 1);
}

std::string Path::extension(const std::string &path) {
  std::string file = filename(path);
  if (file.empty()) {
    return "";
  }

  // Find last dot (but not at start - that's a hidden file)
  size_t pos = file.rfind('.');
  if (pos == std::string::npos || pos == 0) {
    return "";
  }

  return file.substr(pos);
}

} // namespace arcanee::vfs
