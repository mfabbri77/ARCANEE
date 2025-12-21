#include "SysBinding.h"
#include "common/Log.h"
#include "platform/Time.h"
#include "script/BindingUtils.h"
#include <cstdlib>

namespace arcanee::script::api {

SQInteger sys_log(HSQUIRRELVM vm) {
  const SQChar *msg = nullptr;
  if (SQ_FAILED(GetArg(vm, 2, msg)))
    return sq_throwerror(vm, "Invalid argument type for log()");
  LOG_INFO("[Script] %s", msg);
  return 0;
}

SQInteger sys_time(HSQUIRRELVM vm) {
  sq_pushfloat(vm, static_cast<SQFloat>(platform::Time::now()));
  return 1;
}

SQInteger sys_dt(HSQUIRRELVM vm) {
  // Hardcoded for V0.1
  sq_pushfloat(vm, static_cast<SQFloat>(1.0 / 60.0));
  return 1;
}

SQInteger sys_exit(HSQUIRRELVM /*vm*/) {
  LOG_INFO("Script requested exit.");
  // TODO: Implement proper shutdown signal to Runtime
  std::exit(0);
  return 0;
}

void RegisterSysBinding(HSQUIRRELVM vm) {
  sq_pushroottable(vm);
  sq_pushstring(vm, "sys", -1);
  sq_newtable(vm);

  BindFunction(vm, "log", sys_log);
  BindFunction(vm, "time", sys_time);
  BindFunction(vm, "dt", sys_dt);
  BindFunction(vm, "exit", sys_exit);

  sq_newslot(vm, -3, SQTrue); // sys table into root
  sq_pop(vm, 1);              // pop root
}

} // namespace arcanee::script::api
