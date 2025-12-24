#include "BindingHelpers.h"
#include "common/Log.h"
#include <squirrel.h>
#include <string>

namespace arcanee::script {

// Thread-local storage for the last error message
static thread_local std::string g_lastError;

void setLastError(HSQUIRRELVM, const std::string &msg) {
  g_lastError = msg;
  LOG_WARN("Script Error: %s", msg.c_str());
}

SQInteger sys_getLastError(HSQUIRRELVM vm) {
  ARC_BIND_CHECK(vm, checkArity(vm, 0));
  if (g_lastError.empty()) {
    sq_pushnull(vm);
  } else {
    sq_pushstring(vm, g_lastError.c_str(), -1);
  }
  return 1;
}

SQInteger sys_clearLastError(HSQUIRRELVM vm) {
  ARC_BIND_CHECK(vm, checkArity(vm, 0));
  g_lastError.clear();
  return 0;
}

} // namespace arcanee::script
