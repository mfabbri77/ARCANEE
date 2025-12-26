#include "ProjectSystem.h"
#include <algorithm>
#include <iostream>

namespace arcanee::ide {

void FileNode::Sort() {
  std::sort(children.begin(), children.end(),
            [](const FileNode &a, const FileNode &b) {
              if (a.isDirectory != b.isDirectory) {
                return a.isDirectory > b.isDirectory; // Directories first
              }
              return a.name < b.name;
            });
}

ProjectSystem::ProjectSystem() {}

ProjectSystem::~ProjectSystem() {}

Status ProjectSystem::OpenRoot(const std::string &path) {
  if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
    return Status::InvalidArgument("Invalid directory: " + path);
  }

  m_rootPath = std::filesystem::absolute(path).string();
  m_root = FileNode();
  m_root.name = std::filesystem::path(m_rootPath).filename().string();
  m_root.fullPath = m_rootPath;
  m_root.isDirectory = true;

  return ScanDirectory(m_rootPath, m_root);
}

void ProjectSystem::CloseRoot() {
  m_rootPath.clear();
  m_root = FileNode();
}

void ProjectSystem::Refresh() {
  if (!m_rootPath.empty()) {
    ScanDirectory(m_rootPath, m_root);
  }
}

Status ProjectSystem::ScanDirectory(const std::filesystem::path &path,
                                    FileNode &node) {
  node.children.clear();
  try {
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
      FileNode child;
      child.name = entry.path().filename().string();
      child.fullPath = entry.path().string();
      child.isDirectory = entry.is_directory();

      // Basic exclusion
      if (child.name == ".git" || child.name == ".arcanee" ||
          child.name == "build" || child.name == "out") {
        continue;
      }

      if (child.isDirectory) {
        ScanDirectory(entry.path(), child);
      }
      node.children.push_back(child);
    }
    node.Sort();
  } catch (const std::exception &e) {
    return Status::InternalError(std::string("Failed to scan: ") + e.what());
  }
  return Status::Ok();
}

} // namespace arcanee::ide
