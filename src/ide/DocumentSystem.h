#pragma once
#include "TextBuffer.h"
#include "common/Status.h"
#include <functional>
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

// Save listener callback type [REQ-91]
using SaveListener = std::function<void(const std::string &absolute_path)>;

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

  // Save listener management [REQ-91]
  void AddSaveListener(SaveListener listener) {
    m_saveListeners.push_back(std::move(listener));
  }

  void ClearSaveListeners() { m_saveListeners.clear(); }

private:
  std::vector<std::unique_ptr<Document>> m_documents;
  Document *m_activeDoc = nullptr;
  std::vector<SaveListener> m_saveListeners;

  Document *FindDocument(const std::string &path);
  void NotifySaveListeners(const std::string &path);
};

} // namespace arcanee::ide
