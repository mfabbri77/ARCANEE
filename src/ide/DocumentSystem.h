#pragma once
#include "TextBuffer.h"
#include "common/Status.h"
#include <memory>
#include <string>
#include <vector>

namespace arcanee::ide {

struct Document {
  std::string path;
  TextBuffer buffer;
  bool dirty = false;

  // Helper accessors
  std::string filename() const;
};

class DocumentSystem {
public:
  DocumentSystem();
  ~DocumentSystem();

  // Opens a document. If already open, returns existing instance.
  Status OpenDocument(const std::string &path, Document **outDoc);

  // Saving
  Status SaveDocument(Document *doc);

  // Closing
  void CloseDocument(Document *doc);

  // Active document management
  Document *GetActiveDocument() const { return m_activeDoc; }
  void SetActiveDocument(Document *doc) { m_activeDoc = doc; }

  const std::vector<std::unique_ptr<Document>> &GetDocuments() const {
    return m_documents;
  }

private:
  std::vector<std::unique_ptr<Document>> m_documents;
  Document *m_activeDoc = nullptr;

  Document *FindDocument(const std::string &path);
};

} // namespace arcanee::ide
