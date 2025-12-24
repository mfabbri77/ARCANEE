#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace arcanee::ide {

struct Piece {
  enum class Source : uint8_t { Original, Add };

  Source source;
  uint32_t start;
  uint32_t length;
};

struct Cursor {
  uint32_t head = 0;
  uint32_t anchor = 0;
  uint32_t preferredColumn = 0; // For vertical navigation
};

struct EditAction {
  enum class Type { Insert, Delete };
  Type type;
  uint32_t offset;
  std::string text;
  std::vector<Cursor> preCursors;  // State before edit
  std::vector<Cursor> postCursors; // State after edit (mostly for Redo)
  uint64_t timestamp = 0;
  int batchId = 0;
};

class TextBuffer {
public:
  TextBuffer() = default;

  // Load initial content (Original buffer)
  void Load(const std::string &content);

  // Operations
  void Insert(uint32_t offset, const std::string &text);
  void Delete(uint32_t offset, uint32_t length);

  // Undo/Redo
  void Undo();
  void Redo();
  bool CanUndo() const;
  bool CanRedo() const;
  void BeginBatch();
  void EndBatch();

  // Cursor Management
  const std::vector<Cursor> &GetCursors() const { return m_cursors; }
  void AddCursor(uint32_t pos);
  void ClearCursors();
  void SetCursor(uint32_t pos); // Set single cursor
  void SetCursors(const std::vector<Cursor> &cursors);

  // Accessors
  std::string GetAllText() const;
  std::string GetText(uint32_t offset, uint32_t length) const;
  std::string GetLine(int lineIndex) const;
  uint32_t GetLineStart(int lineIndex) const;
  int GetLineFromOffset(uint32_t offset) const;
  int GetLineCount() const;
  uint32_t GetLength() const;

  // Debug
  void PrintPieces() const;

private:
  std::string m_original;
  std::string m_add;

  std::vector<Piece> m_pieces;
  std::vector<Cursor> m_cursors;

  // Undo/Redo Stacks
  std::vector<EditAction> m_undoStack;
  std::vector<EditAction> m_redoStack;
  bool m_isUndoing = false;
  int m_batchDepth = 0;
  int m_currentBatchId = 1;

  // Line Index: Start byte offset of each line in the logical buffer.
  // Last element is essentially total length if we want range.
  std::vector<uint32_t> m_lineIndex;

  void RebuildLineIndex();
  void RecordAction(EditAction::Type type, uint32_t offset,
                    const std::string &text);
};

} // namespace arcanee::ide
