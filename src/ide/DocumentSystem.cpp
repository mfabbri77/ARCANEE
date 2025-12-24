#include "DocumentSystem.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace arcanee::ide {

std::string Document::filename() const {
  return std::filesystem::path(path).filename().string();
}

DocumentSystem::DocumentSystem() {}
DocumentSystem::~DocumentSystem() {}

Status DocumentSystem::OpenDocument(const std::string &path,
                                    Document **outDoc) {
  if (outDoc)
    *outDoc = nullptr;

  std::string absPath = std::filesystem::absolute(path).string();

  // Check if already open
  if (Document *existing = FindDocument(absPath)) {
    if (outDoc)
      *outDoc = existing;
    return Status::Ok();
  }

  if (!std::filesystem::exists(absPath)) {
    return Status::NotFound("File not found: " + absPath);
  }

  // Read file
  std::ifstream file(
      absPath, std::ios::in |
                   std::ios::binary); // Binary to handle EOL manually if needed
  if (!file.is_open()) {
    return Status::InternalError("Failed to open file definition");
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  auto newDoc = std::make_unique<Document>();
  newDoc->path = absPath;
  newDoc->buffer.Load(buffer.str());
  newDoc->dirty = false;

  Document *ptr = newDoc.get();
  m_documents.push_back(std::move(newDoc));

  if (outDoc)
    *outDoc = ptr;
  return Status::Ok();
}

Status DocumentSystem::SaveDocument(Document *doc) {
  if (!doc)
    return Status::InvalidArgument("Invalid document");

  std::ofstream file(doc->path,
                     std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    return Status::InternalError("Failed to open file for writing: " +
                                 doc->path);
  }

  std::string content = doc->buffer.GetAllText();
  file.write(content.c_str(), content.size());
  doc->dirty = false;

  return Status::Ok();
}

void DocumentSystem::CloseDocument(Document *doc) {
  if (!doc)
    return;

  auto it = std::find_if(
      m_documents.begin(), m_documents.end(),
      [doc](const std::unique_ptr<Document> &d) { return d.get() == doc; });

  if (it != m_documents.end()) {
    if (m_activeDoc == doc) {
      m_activeDoc = nullptr;
      // Select another? logic later
    }
    m_documents.erase(it);
  }
}

Document *DocumentSystem::FindDocument(const std::string &path) {
  for (const auto &doc : m_documents) {
    if (doc->path == path)
      return doc.get();
  }
  return nullptr;
}

} // namespace arcanee::ide
