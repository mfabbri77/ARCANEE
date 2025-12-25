#include "BreakpointStore.h"
#include "common/Log.h"
#include <algorithm>

namespace arcanee::script {

void BreakpointStore::add(const std::string &file, int line) {
  if (hasBreakpoint(file, line))
    return;

  m_lookup[file].insert(line);
  m_linear.push_back({file, line, true});
  LOG_INFO("Breakpoint added: %s:%d", file.c_str(), line);
}

void BreakpointStore::remove(const std::string &file, int line) {
  if (m_lookup.find(file) != m_lookup.end()) {
    m_lookup[file].erase(line);
    if (m_lookup[file].empty()) {
      m_lookup.erase(file);
    }
  }

  // Remove from linear view
  m_linear.erase(std::remove_if(m_linear.begin(), m_linear.end(),
                                [&](const DebugBreakpoint &bp) {
                                  return bp.file == file && bp.line == line;
                                }),
                 m_linear.end());
  LOG_INFO("Breakpoint removed: %s:%d", file.c_str(), line);
}

void BreakpointStore::clear() {
  m_lookup.clear();
  m_linear.clear();
}

bool BreakpointStore::hasBreakpoint(const std::string &file, int line) const {
  // 1. Exact Match (Fast)
  auto it = m_lookup.find(file);
  if (it != m_lookup.end()) {
    if (it->second.count(line) > 0)
      return true;
  }

  // 2. Linear Scan Fallback (Robust VFS/Absolute matching)
  // This handles cases where BP is set on "/abs/path/main.nut" but VM reports
  // "cart:/main.nut"
  for (const auto &bp : m_linear) {
    if (bp.line != line || !bp.enabled)
      continue;

    // Check 2a: Suffix match (e.g. "main.nut" matches "/path/to/main.nut")
    // If one contains the other
    bool match = false;

    // Check if filename part matches
    size_t sep1 = file.find_last_of("/\\");
    std::string fn1 =
        (sep1 == std::string::npos) ? file : file.substr(sep1 + 1);

    size_t sep2 = bp.file.find_last_of("/\\");
    std::string fn2 =
        (sep2 == std::string::npos) ? bp.file : bp.file.substr(sep2 + 1);

    if (fn1 == fn2) {
      // Filenames match. Now ensure we aren't matching "a/foo.nut" with
      // "b/foo.nut" if enough context exists. For now, in this project
      // structure, filename match is usually sufficient and safe enough.
      return true;
    }
  }

  return false;
}

const std::vector<DebugBreakpoint> &BreakpointStore::getAll() const {
  return m_linear;
}

void BreakpointStore::rebuildLinear() {
  // Not used yet, we keep sync in add/remove
}

} // namespace arcanee::script
