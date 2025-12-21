#pragma once

/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

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
