#pragma once

#include "common/Status.h"
#include <squirrel.h>
#include <string>

namespace arcanee::script {

// Helper to set last error in the VM/Runtime context
void setLastError(HSQUIRRELVM vm, const std::string &msg);

// Native bindings for sys.getLastError / sys.clearLastError
SQInteger sys_getLastError(HSQUIRRELVM vm);
SQInteger sys_clearLastError(HSQUIRRELVM vm);

// --- Argument Checking Helpers ---

// Check argument count (exact)
inline Status checkArity(HSQUIRRELVM vm, SQInteger expected) {
  SQInteger top = sq_gettop(vm);
  // Stack includes 'this' at index 1 for closures, arguments start at 2.
  // Wait, sq_gettop returns total args including 'this' if it's a closure/class
  // method? For standard native closures: index 1 is 'this' (root table or
  // object), 2..N are args. So expected N args means top should be N + 1.
  if (top != expected + 1) {
    return Status::InvalidArgument("Expected " + std::to_string(expected) +
                                   " arguments");
  }
  return Status::Ok();
}

// Get Integer at index (idx is absolute, e.g. 2 for first arg)
inline StatusOr<SQInteger> getInt(HSQUIRRELVM vm, SQInteger idx,
                                  const std::string &name) {
  SQInteger val;
  if (SQ_FAILED(sq_getinteger(vm, idx, &val))) {
    return Status::InvalidArgument(name + " must be an integer");
  }
  return val;
}

// Get Float at index
inline StatusOr<SQFloat> getFloat(HSQUIRRELVM vm, SQInteger idx,
                                  const std::string &name) {
  SQFloat val;
  if (SQ_FAILED(sq_getfloat(vm, idx, &val))) {
    // Try int? squirrel usually converts int to float if requested?
    // sq_getfloat might fail if it's strictly integer? No, usually it converts.
    // Let's rely on standard squirrel behavior.
    return Status::InvalidArgument(name + " must be a number");
  }
  return val;
}

// Get String at index
inline StatusOr<const SQChar *> getString(HSQUIRRELVM vm, SQInteger idx,
                                          const std::string &name) {
  const SQChar *val;
  if (SQ_FAILED(sq_getstring(vm, idx, &val))) {
    return Status::InvalidArgument(name + " must be a string");
  }
  return val;
}

// Get Bool
inline StatusOr<bool> getBool(HSQUIRRELVM vm, SQInteger idx,
                              const std::string &name) {
  SQBool val;
  if (SQ_FAILED(sq_getbool(vm, idx, &val))) {
    // Strict bool check? Or allow falsy/truthy?
    // sq_getbool usually only works on BOOL type.
    // For loose handling, we might check type.
    // For now strict.
    return Status::InvalidArgument(name + " must be a boolean");
  }
  return val == SQTrue;
}

// --- Boilerplate Macros ---

// Fails the binding if status is not OK.
// In v0.1: Returns false/null to script and sets Sys.LastError via helper.
// Actually standard squirrel binding returns SQInteger (number of return
// values) or throws. Spec says: "On failure, return false/null and set
// sys.getLastError()". So we should push false/null and return 1.
#define ARC_BIND_CHECK(vm, expr)                                               \
  do {                                                                         \
    arcanee::Status _s = (expr);                                               \
    if (!_s.ok()) {                                                            \
      arcanee::script::setLastError(vm, _s.message());                         \
      sq_pushnull(vm); /* Default failure return: null */                      \
      return 1;                                                                \
    }                                                                          \
  } while (0)

// Helper for void functions: if expr returns status error, handle it.
// If success, push void (no return value? Or null?).
// Spec says "void functions become no-ops but MUST set last-error".
// If they return nothing in script (void), we return 0 vals.
#define ARC_BIND_VOID(vm, expr)                                                \
  do {                                                                         \
    arcanee::Status _s = (expr);                                               \
    if (!_s.ok()) {                                                            \
      arcanee::script::setLastError(vm, _s.message());                         \
      return 0;                                                                \
    }                                                                          \
    return 0;                                                                  \
  } while (0)

// For functions returning values (StatusOr<T>)
// T must be pushable.
// If error, return null (1 return val).
#define ARC_BIND_RET(vm, expr, pushFunc)                                       \
  do {                                                                         \
    auto _res = (expr);                                                        \
    if (!_res.ok()) {                                                          \
      arcanee::script::setLastError(vm, _res.status().message());              \
      sq_pushnull(vm);                                                         \
      return 1;                                                                \
    }                                                                          \
    pushFunc(vm, _res.value());                                                \
    return 1;                                                                  \
  } while (0)

} // namespace arcanee::script
