#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arcanee::script {

struct DebugBreakpoint {
  std::string file; // Canonical VFS path
  int line;
  bool enabled = true;
};

class BreakpointStore {
public:
  void add(const std::string &file, int line);
  void remove(const std::string &file, int line);
  void clear();

  bool hasBreakpoint(const std::string &file, int line) const;

  // Returns all breakpoints (for UI/DAP enumeration)
  const std::vector<DebugBreakpoint> &getAll() const;

private:
  // Fast lookup: File -> Set of Lines
  std::unordered_map<std::string, std::unordered_set<int>> m_lookup;

  // Linear storage for API compatibility / iteration
  std::vector<DebugBreakpoint> m_linear;

  void rebuildLinear();
};

} // namespace arcanee::script
