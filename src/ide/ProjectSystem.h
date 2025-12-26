#pragma once
#include "common/Status.h"
#include <filesystem>
#include <string>
#include <vector>

namespace arcanee::ide {

struct FileNode {
  std::string name;
  std::string fullPath;
  bool isDirectory = false;
  std::vector<FileNode> children;

  // Helper to sort children (directories first, then files)
  void Sort();
};

class ProjectSystem {
public:
  ProjectSystem();
  ~ProjectSystem();

  // Sets the workspace root and scans it
  Status OpenRoot(const std::string &path);

  // Closes the current root
  void CloseRoot();

  // Re-scans the current root
  void Refresh();

  const FileNode &GetRoot() const { return m_root; }
  const std::string &GetRootPath() const { return m_rootPath; }
  bool HasProject() const { return !m_rootPath.empty(); }

private:
  std::string m_rootPath;
  FileNode m_root;

  Status ScanDirectory(const std::filesystem::path &path, FileNode &node);
};

} // namespace arcanee::ide
