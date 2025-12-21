#pragma once

#include "Log.h"
#include <cstdlib>

namespace arcanee {

// Assert macros for development (Chapter 11)
#ifdef NDEBUG
#define ARCANEE_ASSERT(condition, msg)
#define ARCANEE_VERIFY(condition, msg) (condition)
#else
#define ARCANEE_ASSERT(condition, msg)                                         \
  do {                                                                         \
    if (!(condition)) {                                                        \
      LOG_FATAL("Assertion failed: %s\n  at %s:%d\n  %s", #condition,          \
                __FILE__, __LINE__, msg);                                      \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

#define ARCANEE_VERIFY(condition, msg)                                         \
  do {                                                                         \
    if (!(condition)) {                                                        \
      LOG_FATAL("Verification failed: %s\n  at %s:%d\n  %s", #condition,       \
                __FILE__, __LINE__, msg);                                      \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
#endif

} // namespace arcanee
