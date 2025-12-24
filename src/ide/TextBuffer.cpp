#include "TextBuffer.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <iterator>

namespace arcanee::ide {

void TextBuffer::Load(const std::string &content) {
  m_original = content;
  m_add.clear();
  m_pieces.clear();

  Piece p;
  p.source = Piece::Source::Original;
  p.start = 0;
  p.length = (uint32_t)m_original.size();

  if (p.length > 0) {
    m_pieces.push_back(p);
  }

  RebuildLineIndex();
  SetCursor(0);
}

void TextBuffer::AddCursor(uint32_t pos) {
  Cursor c;
  c.head = pos;
  c.anchor = pos;
  c.preferredColumn = 0; // TODO: Calculate column from pos
  m_cursors.push_back(c);
}

void TextBuffer::ClearCursors() { m_cursors.clear(); }

void TextBuffer::SetCursor(uint32_t pos) {
  ClearCursors();
  AddCursor(pos);
}

uint32_t TextBuffer::GetLength() const {
  uint32_t len = 0;
  for (const auto &p : m_pieces)
    len += p.length;
  return len;
}

std::string TextBuffer::GetAllText() const {
  std::string out;
  out.reserve(GetLength());
  for (const auto &p : m_pieces) {
    if (p.source == Piece::Source::Original) {
      out.append(m_original, p.start, p.length);
    } else {
      out.append(m_add, p.start, p.length);
    }
  }
  return out;
}

void TextBuffer::Insert(uint32_t offset, const std::string &text) {
  if (text.empty())
    return;

  // Create new piece info
  Piece newPiece;
  newPiece.source = Piece::Source::Add;
  newPiece.start = (uint32_t)m_add.size();
  newPiece.length = (uint32_t)text.size();

  m_add += text;

  if (m_pieces.empty()) {
    m_pieces.push_back(newPiece);
  } else {
    // Find split point
    uint32_t currentPos = 0;
    bool inserted = false;
    for (auto it = m_pieces.begin(); it != m_pieces.end(); ++it) {
      if (offset >= currentPos && offset < currentPos + it->length) {
        uint32_t relOffset = offset - currentPos;

        if (relOffset == 0) {
          m_pieces.insert(it, newPiece);
        } else {
          // Split
          Piece left = *it;
          left.length = relOffset;

          Piece right = *it;
          right.start += relOffset;
          right.length -= relOffset;

          *it = left;
          it = m_pieces.insert(it + 1, newPiece);
          m_pieces.insert(it + 1, right);
        }
        inserted = true;
        break;
      }
      currentPos += it->length;
    }
    if (!inserted && offset == currentPos) {
      m_pieces.push_back(newPiece);
    }
  }

  RebuildLineIndex();

  // Record Action
  if (!m_isUndoing) {
    RecordAction(EditAction::Type::Insert, offset, text);
    m_redoStack.clear();
  }
}

std::string TextBuffer::GetText(uint32_t offset, uint32_t length) const {
  if (length == 0)
    return "";
  uint32_t endOffset = offset + length;

  std::string text;
  text.reserve(length);

  uint32_t cur = 0;
  for (const auto &p : m_pieces) {
    if (cur + p.length <= offset) {
      cur += p.length;
      continue;
    }
    if (cur >= endOffset)
      break;

    uint32_t startInPiece = (offset > cur) ? (offset - cur) : 0;
    uint32_t endInPiece =
        (endOffset < cur + p.length) ? (endOffset - cur) : p.length;

    const std::string &buf =
        (p.source == Piece::Source::Original) ? m_original : m_add;
    text.append(buf, p.start + startInPiece, endInPiece - startInPiece);
    cur += p.length;
  }
  return text;
}

void TextBuffer::Delete(uint32_t offset, uint32_t length) {
  if (length == 0)
    return;
  uint32_t endOffset = offset + length;

  // Capture deleted text for Undo if recording
  if (!m_isUndoing) {
    std::string deletedText = GetText(offset, length);
    RecordAction(EditAction::Type::Delete, offset, deletedText);
    m_redoStack.clear();
  }

  std::vector<Piece> newPieces;
  uint32_t currentPos = 0;

  for (const auto &p : m_pieces) {
    uint32_t pStart = currentPos;
    uint32_t pEnd = currentPos + p.length;

    // Piece is completely before deletion
    if (pEnd <= offset) {
      newPieces.push_back(p);
    }
    // Piece is completely after deletion
    else if (pStart >= endOffset) {
      newPieces.push_back(p);
    } else {
      // Overlap
      // Keep Left part?
      if (pStart < offset) {
        Piece left = p;
        left.length = offset - pStart;
        newPieces.push_back(left);
      }
      // Keep Right part?
      if (pEnd > endOffset) {
        Piece right = p;
        uint32_t cut = endOffset - pStart;
        right.start += cut;
        right.length -= cut;
        newPieces.push_back(right);
      }
    }
    currentPos += p.length;
  }
  m_pieces = newPieces;
  RebuildLineIndex();
}

void TextBuffer::Undo() {
  if (!CanUndo())
    return;

  m_isUndoing = true;
  int batch = m_undoStack.back().batchId;

  do {
    EditAction action = m_undoStack.back();
    m_undoStack.pop_back();

    // Reverse Action
    if (action.type == EditAction::Type::Insert) {
      Delete(action.offset, (uint32_t)action.text.size());
    } else {
      Insert(action.offset, action.text);
    }

    SetCursors(action.preCursors);

    // Push to Redo
    action.postCursors =
        m_cursors; // Capture state after undo as "post" for redo? No, Redo
                   // needs to end up here. Wait, Redo executes original action.
                   // Result state is postCursors.
    m_redoStack.push_back(action);

  } while (batch != 0 && !m_undoStack.empty() &&
           m_undoStack.back().batchId == batch);

  m_isUndoing = false;
}

void TextBuffer::Redo() {
  if (!CanRedo())
    return;

  m_isUndoing = true;
  int batch = m_redoStack.back().batchId;

  do {
    EditAction action = m_redoStack.back();
    m_redoStack.pop_back();

    if (action.type == EditAction::Type::Insert) {
      Insert(action.offset, action.text);
    } else {
      Delete(action.offset, (uint32_t)action.text.size());
    }

    // Restore cursors? Usually post-action state.
    // Or let new action set it? We rely on recorded.
    // We didn't record postCursors during original action though.
    // Let's rely on manual cursor set or logic.
    // For now, minimal.

    // Push back to Undo
    m_undoStack.push_back(action);

  } while (batch != 0 && !m_redoStack.empty() &&
           m_redoStack.back().batchId == batch);

  m_isUndoing = false;
}

bool TextBuffer::CanUndo() const { return !m_undoStack.empty(); }
bool TextBuffer::CanRedo() const { return !m_redoStack.empty(); }

void TextBuffer::BeginBatch() {
  m_batchDepth++;
  if (m_batchDepth == 1)
    m_currentBatchId++;
}
void TextBuffer::EndBatch() {
  if (m_batchDepth > 0)
    m_batchDepth--;
}

void TextBuffer::RecordAction(EditAction::Type type, uint32_t offset,
                              const std::string &text) {
  EditAction action;
  action.type = type;
  action.offset = offset;
  action.text = text;
  action.preCursors = m_cursors;
  action.batchId = (m_batchDepth > 0) ? m_currentBatchId : 0;
  m_undoStack.push_back(action);
}

void TextBuffer::SetCursors(const std::vector<Cursor> &cursors) {
  m_cursors = cursors;
}

void TextBuffer::RebuildLineIndex() {
  m_lineIndex.clear();
  m_lineIndex.push_back(0); // Line 0 starts at 0

  uint32_t currentByte = 0;

  // Helper to scan chars
  auto handleChar = [&](char c) {
    currentByte++;
    if (c == '\n') {
      m_lineIndex.push_back(currentByte);
    }
  };

  for (const auto &p : m_pieces) {
    const std::string &buffer =
        (p.source == Piece::Source::Original) ? m_original : m_add;
    for (uint32_t i = 0; i < p.length; ++i) {
      handleChar(buffer[p.start + i]);
    }
  }
}

int TextBuffer::GetLineCount() const {
  if (m_lineIndex.empty())
    return 1;
  // If last char is \n, we have extra line?
  // Usually line count is lineIndex.size().
  // If text is "A", idx=[0], size=1.
  // If text is "A\n", idx=[0, 2], size=2.
  // If file is empty, idx=[0], size=1.
  // Seems correct for 1-based logic or 0-based index.
  return (int)m_lineIndex.size();
}

std::string TextBuffer::GetLine(int lineIndex) const {
  if (lineIndex < 0 || lineIndex >= (int)m_lineIndex.size())
    return "";

  uint32_t start = m_lineIndex[lineIndex];
  uint32_t end;
  if (lineIndex + 1 < (int)m_lineIndex.size()) {
    end = m_lineIndex[lineIndex + 1] - 1; // Exclude \n? Or include?
    // Usually GetLine returns content without eol, or with.
    // InputTextMultiline expects embedded newlines if it's full buffer.
    // Here we render line by line. ImGui::Text doesn't care about \n at end,
    // but we usually strip it for clean rendering if we draw custom. Let's
    // include it for now to be safe, logic can trim.
    end = m_lineIndex[lineIndex + 1];
    // Note: the index points to START of next line (char AFTER \n).
    // So range is [start, index_next).
  } else {
    end = GetLength();
  }

  if (start >= end)
    return "";

  std::string out;
  out.reserve(end - start);

  // Extract range [start, end)
  uint32_t currentPos = 0;
  for (const auto &p : m_pieces) {
    uint32_t pStart = currentPos;
    uint32_t pEnd = currentPos + p.length;

    if (pEnd <= start) {
      currentPos += p.length;
      continue;
    }
    if (pStart >= end)
      break;

    // Overlap
    uint32_t copyStart = (start > pStart) ? (start - pStart) : 0;
    uint32_t copyEnd = (end < pEnd) ? (end - pStart) : p.length;

    const std::string &buffer =
        (p.source == Piece::Source::Original) ? m_original : m_add;
    out.append(buffer, p.start + copyStart, copyEnd - copyStart);

    currentPos += p.length;
  }

  // Strip trailing \n if present for display?
  if (!out.empty() && out.back() == '\n')
    out.pop_back();

  return out;
}

uint32_t TextBuffer::GetLineStart(int lineIndex) const {
  if (lineIndex < 0)
    return 0;
  if (lineIndex >= (int)m_lineIndex.size()) {
    if (m_lineIndex.empty())
      return 0;
    return m_lineIndex.back(); // Often acceptable to return EOF pos
  }
  return m_lineIndex[lineIndex];
}

int TextBuffer::GetLineFromOffset(uint32_t offset) const {
  if (m_lineIndex.empty())
    return 0;
  auto it = std::upper_bound(m_lineIndex.begin(), m_lineIndex.end(), offset);
  if (it == m_lineIndex.begin())
    return 0;
  return (int)std::distance(m_lineIndex.begin(), it) - 1;
}

void TextBuffer::PrintPieces() const {
  std::cout << "Buffer Pieces:\n";
  for (const auto &p : m_pieces) {
    std::cout << "  [" << (p.source == Piece::Source::Original ? "ORG" : "ADD")
              << "] Start:" << p.start << " Len:" << p.length << "\n";
  }
}

int TextBuffer::Find(const std::string &needle, uint32_t startOffset,
                     bool caseSensitive) const {
  if (needle.empty())
    return -1;

  std::string haystack = GetAllText();
  if (startOffset >= haystack.size())
    return -1;

  if (caseSensitive) {
    size_t pos = haystack.find(needle, startOffset);
    return (pos != std::string::npos) ? (int)pos : -1;
  } else {
    // Case-insensitive search
    std::string lowerHaystack = haystack;
    std::string lowerNeedle = needle;
    std::transform(lowerHaystack.begin(), lowerHaystack.end(),
                   lowerHaystack.begin(), ::tolower);
    std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(),
                   ::tolower);
    size_t pos = lowerHaystack.find(lowerNeedle, startOffset);
    return (pos != std::string::npos) ? (int)pos : -1;
  }
}

std::vector<uint32_t> TextBuffer::FindAll(const std::string &needle,
                                          bool caseSensitive) const {
  std::vector<uint32_t> results;
  if (needle.empty())
    return results;

  int pos = 0;
  while ((pos = Find(needle, pos, caseSensitive)) >= 0) {
    results.push_back((uint32_t)pos);
    pos += 1; // Move past current match to find next
  }
  return results;
}

bool TextBuffer::Replace(uint32_t offset, uint32_t length,
                         const std::string &replacement) {
  if (offset > GetLength())
    return false;

  BeginBatch();
  Delete(offset, length);
  Insert(offset, replacement);
  EndBatch();
  return true;
}

int TextBuffer::ReplaceAll(const std::string &needle,
                           const std::string &replacement, bool caseSensitive) {
  if (needle.empty())
    return 0;

  auto matches = FindAll(needle, caseSensitive);
  if (matches.empty())
    return 0;

  BeginBatch();

  // Replace from end to start to maintain offsets
  int count = 0;
  for (int i = (int)matches.size() - 1; i >= 0; i--) {
    Delete(matches[i], (uint32_t)needle.size());
    Insert(matches[i], replacement);
    count++;
  }

  EndBatch();
  return count;
}

} // namespace arcanee::ide
