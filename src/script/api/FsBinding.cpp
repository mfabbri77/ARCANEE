#include "FsBinding.h"
#include "common/Log.h"
#include "script/BindingUtils.h"
#include "script/ScriptEngine.h"
#include "vfs/Vfs.h"

namespace arcanee::script::api {

namespace {

vfs::IVfs *GetVfs(HSQUIRRELVM vm) {
  auto *engine = static_cast<ScriptEngine *>(sq_getforeignptr(vm));
  if (!engine)
    return nullptr;
  return engine->getVfs();
}

} // namespace

SQInteger fs_exists(HSQUIRRELVM vm) {
  const SQChar *path;
  if (SQ_FAILED(GetArg(vm, 2, path)))
    return sq_throwerror(vm, "Invalid argument: expected string path");

  auto *vfs = GetVfs(vm);
  if (!vfs)
    return sq_throwerror(vm, "VFS not initialized");

  sq_pushbool(vm, vfs->exists(path) ? SQTrue : SQFalse);
  return 1;
}

SQInteger fs_read(HSQUIRRELVM vm) {
  const SQChar *path;
  if (SQ_FAILED(GetArg(vm, 2, path)))
    return sq_throwerror(vm, "Invalid argument: expected string path");

  auto *vfs = GetVfs(vm);
  if (!vfs)
    return sq_throwerror(vm, "VFS not initialized");

  auto content = vfs->readText(path);
  if (!content)
    return sq_throwerror(vm, "File not found or read error");

  sq_pushstring(vm, content->c_str(), -1);
  return 1;
}

SQInteger fs_write(HSQUIRRELVM vm) {
  const SQChar *path;
  const SQChar *content;
  if (SQ_FAILED(GetArg(vm, 2, path)))
    return sq_throwerror(vm, "Invalid path: expected string");
  if (SQ_FAILED(GetArg(vm, 3, content)))
    return sq_throwerror(vm, "Invalid content: expected string");

  auto *vfs = GetVfs(vm);
  if (!vfs)
    return sq_throwerror(vm, "VFS not initialized");

  if (vfs->writeText(path, content) == vfs::VfsError::None) {
    sq_pushbool(vm, SQTrue);
  } else {
    // Might want to return false instead of error for runtime failures
    sq_pushbool(vm, SQFalse);
  }
  return 1;
}

void RegisterFsBinding(HSQUIRRELVM vm) {
  sq_pushroottable(vm);
  sq_pushstring(vm, "fs", -1);
  sq_newtable(vm);

  BindFunction(vm, "exists", fs_exists);
  BindFunction(vm, "read", fs_read);
  BindFunction(vm, "write", fs_write);

  sq_newslot(vm, -3, SQTrue); // fs table into root
  sq_pop(vm, 1);              // pop root
}

} // namespace arcanee::script::api
