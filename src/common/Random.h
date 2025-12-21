#pragma once

#include "Types.h"

namespace arcanee {

// Xorshift128+ RNG - Normative reference implementation from Chapter 1 §1.6.2
// Provides deterministic, portable random number generation for cartridge use
class Xorshift128Plus {
public:
  Xorshift128Plus(u64 seed = 1) { setSeed(seed); }

  // Initialize both state words deterministically from seed (normative)
  void setSeed(u64 seed) {
    // Deterministic initialization using splitmix64
    m_state[0] = splitmix64(seed);
    m_state[1] = splitmix64(m_state[0]);
    // Ensure non-zero state
    if (m_state[0] == 0 && m_state[1] == 0) {
      m_state[0] = 1;
    }
  }

  // Generate next 64-bit random value
  u64 next() {
    u64 s1 = m_state[0];
    u64 s0 = m_state[1];
    m_state[0] = s0;
    s1 ^= s1 << 23;
    m_state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return m_state[1] + s0;
  }

  // sys.rand() MUST return integer in range [0, 2^31 - 1] (normative)
  i32 randInt() { return static_cast<i32>((next() >> 33) & 0x7FFFFFFF); }

  // Random float in range [0, 1)
  f64 randFloat() {
    return static_cast<f64>(next() >> 11) * (1.0 / 9007199254740992.0);
  }

  // Random integer in range [min, max] inclusive
  i32 randRange(i32 min, i32 max) {
    if (min > max) {
      i32 tmp = min;
      min = max;
      max = tmp;
    }
    u64 range = static_cast<u64>(max) - static_cast<u64>(min) + 1;
    return min + static_cast<i32>(next() % range);
  }

private:
  u64 m_state[2];

  // Splitmix64 for seed initialization
  static u64 splitmix64(u64 x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
  }
};

} // namespace arcanee
