#include "TextBuffer.h"
#include <algorithm>
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
    RebuildLineIndex();
    return;
  }

  // Find split point
  uint32_t currentPos = 0;
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
      RebuildLineIndex();
      return;
    }
    currentPos += it->length;
  }

  // Append if at end
  if (offset == currentPos) {
    m_pieces.push_back(newPiece);
  }
  RebuildLineIndex();
}

void TextBuffer::Delete(uint32_t offset, uint32_t length) {
  if (length == 0)
    return;
  uint32_t endOffset = offset + length;

  // Naive implementation: Iterate, split/shorten/remove pieces in range
  // A better approach usually involves building a new piece list for the
  // affected range. For MVP, we'll do simple split logic, recursively or
  // iteratively. Actually, simplest is: Find First Piece affected, Find Last
  // Piece affected. Fix boundaries, remove middle ones.

  // IMPLEMENTATION DEFERRED slightly or simplified for step-1 check.
  // Let's implement full logic:

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

} // namespace arcanee::ide
