/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * @file HandlePool.h
 */

#pragma once

#include "common/Assert.h"
#include <cstdint>
#include <vector>

namespace arcanee::script {

using Handle = int32_t;

constexpr Handle INVALID_HANDLE = -1;

/**
 * @brief Logic-less container for managing handles to resources.
 * Validates ownership and liveness using generation counts.
 *
 * Handle Structure (32-bit):
 * [31-16] Generation (16 bits)
 * [15-00] Index (16 bits)
 */
template <typename T> class HandlePool {
public:
  struct Entry {
    T *object = nullptr;
    uint16_t generation = 0;
    bool free = true;
  };

  Handle add(T *obj) {
    ARCANEE_ASSERT(obj != nullptr, "Cannot add null object to pool");

    uint16_t idx = 0;
    if (m_freeIndices.empty()) {
      if (m_entries.size() >= 0xFFFF) {
        // Pool full
        return INVALID_HANDLE;
      }
      m_entries.emplace_back();
      idx = static_cast<uint16_t>(m_entries.size() - 1);
    } else {
      idx = m_freeIndices.back();
      m_freeIndices.pop_back();
    }

    Entry &e = m_entries[idx];
    e.object = obj;
    e.free = false;
    // Generation is already incremented on remove, or 0 for new

    return makeHandle(idx, e.generation);
  }

  T *get(Handle h) {
    if (h == INVALID_HANDLE)
      return nullptr;

    uint16_t idx = getIndex(h);
    uint16_t gen = getGeneration(h);

    if (idx >= m_entries.size())
      return nullptr;

    Entry &e = m_entries[idx];
    if (e.free || e.generation != gen) {
      return nullptr;
    }

    return e.object;
  }

  void remove(Handle h) {
    if (h == INVALID_HANDLE)
      return;

    uint16_t idx = getIndex(h);
    uint16_t gen = getGeneration(h);

    if (idx >= m_entries.size())
      return; // Invalid handle

    Entry &e = m_entries[idx];
    if (e.free || e.generation != gen) {
      return; // Double free or invalid
    }

    // Destroy object
    delete e.object;
    e.object = nullptr;

    // Mark free and increment generation
    e.free = true;
    e.generation++;

    m_freeIndices.push_back(idx);
  }

  bool isValid(Handle h) { return get(h) != nullptr; }

  void clear() {
    for (auto &e : m_entries) {
      if (!e.free && e.object) {
        delete e.object;
      }
    }
    m_entries.clear();
    m_freeIndices.clear();
  }

  ~HandlePool() { clear(); }

private:
  std::vector<Entry> m_entries;
  std::vector<uint16_t> m_freeIndices;

  Handle makeHandle(uint16_t idx, uint16_t gen) {
    return (int32_t)((gen << 16) | idx);
  }

  uint16_t getIndex(Handle h) { return (uint16_t)(h & 0xFFFF); }

  uint16_t getGeneration(Handle h) { return (uint16_t)((h >> 16) & 0xFFFF); }
};

} // namespace arcanee::script
