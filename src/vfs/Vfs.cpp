#include "Vfs.h"
#include "common/Log.h"
#include <algorithm>
#include <sstream>

namespace arcanee::vfs {

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
  case VfsError::PermissionDenied:
    return "Permission denied";
  case VfsError::QuotaExceeded:
    return "Storage quota exceeded";
  case VfsError::IoError:
    return "I/O error";
  case VfsError::NotInitialized:
    return "VFS not initialized";
  }
  return "Unknown error";
}

// Path implementation based on Chapter 3 §3.6.1.1
std::optional<ParsedPath> Path::parse(const std::string &input) {
  // Step 1: Find namespace separator ":/
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

  // Step 3: Extract path after ":/""
  std::string path = input.substr(colonPos + 2);

  // Step 4: Replace backslashes with forward slashes
  std::replace(path.begin(), path.end(), '\\', '/');

  // Step 5: Split into segments and process
  std::vector<std::string> segments;
  std::istringstream stream(path);
  std::string segment;

  while (std::getline(stream, segment, '/')) {
    if (segment.empty() || segment == ".") {
      continue; // Skip empty and current-dir segments
    }
    if (segment == "..") {
      LOG_DEBUG("Path::parse: parent traversal (..) rejected in '%s'",
                input.c_str());
      return std::nullopt; // REJECT parent traversal (normative)
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

} // namespace arcanee::vfs
