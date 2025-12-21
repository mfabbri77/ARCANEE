/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file ScriptEngine.cpp
 */

#include "ScriptEngine.h"
#include "api/AudioBinding.h"
#include "api/FsBinding.h"
#include "api/GfxBinding.h"
#include "api/InputBinding.h"
#include "api/SysBinding.h"
#include "common/Assert.h"
#include "common/Log.h"
#include "platform/Time.h"
#include "vfs/Vfs.h"
#include <cstdarg>
#include <optional>
#include <sqstdaux.h>
#include <sqstdblob.h>
#include <sqstdio.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdsystem.h>

namespace arcanee::script {

namespace {

// Error print callback (routes to LOG_ERROR)
// Output print callback (routes to LOG_INFO)
void printFunc(HSQUIRRELVM /*v*/, const SQChar *s, ...) {
  va_list args;
  va_start(args, s);

  // Buffer output to avoid partial lines
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), s, args);

  // LOG_INFO already handles newlines, so we might want to trim trailing \n
  // for cleaner output, but for now let's just log.
  // Note: Squirrel often prints chunks. Ideally we'd buffer until \n.
  // For v0.1 specific use cases like ::print("foo\n"), let's just log it.

  LOG_INFO("[Script] %s", buffer);

  va_end(args);
}

void errorFunc(HSQUIRRELVM /*v*/, const SQChar *s, ...) {
  va_list args;
  va_start(args, s);
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
}

// Custom compiler error handler
void compilerErrorFunc(HSQUIRRELVM /*v*/, const SQChar *desc,
                       const SQChar *source, SQInteger line, SQInteger column) {
  LOG_ERROR("Script Error: %s\n  at %s:%lld:%lld", desc, source, line, column);
}
} // namespace

// Custom runtime error handler to print stack trace
SQInteger runtimeErrorHandler(HSQUIRRELVM v) {
  const SQChar *sErr = nullptr;
  if (SQ_SUCCEEDED(sq_getstring(v, 2, &sErr))) {
    LOG_ERROR("Script Runtime Error: %s", sErr);
  } else {
    LOG_ERROR("Script Runtime Error: <unknown>");
  }

  // Walk stack
  SQInteger level = 1; // Start from 1 to skip error handler itself
  SQStackInfos si;
  while (SQ_SUCCEEDED(sq_stackinfos(v, level, &si))) {
    const SQChar *fn = si.funcname ? si.funcname : "<unknown>";
    const SQChar *src = si.source ? si.source : "<unknown>";
    LOG_ERROR("  at %s (%s:%lld)", fn, src, si.line);
    level++;
  }

  return 0;
}

// Watchdog hook
void debugHook(HSQUIRRELVM v, SQInteger /*type*/, const SQChar * /*sourcename*/,
               SQInteger /*line*/, const SQChar * /*funcname*/) {
  ScriptEngine *engine = (ScriptEngine *)sq_getforeignptr(v);
  if (engine && engine->m_watchdogEnabled) {
    double elapsed = platform::Time::now() - engine->m_executionStartTime;
    if (elapsed > engine->m_watchdogTimeout) {
      sq_throwerror(v, "Watchdog timeout: Execution time limit exceeded");
    }
  }
}

class ScopedWatchdog {
public:
  explicit ScopedWatchdog(ScriptEngine *engine) : m_engine(engine) {
    if (m_engine->m_watchdogEnabled) {
      m_engine->m_executionStartTime = platform::Time::now();
      // Hook on lines to catch infinite loops (while(true))
      sq_setnativedebughook(m_engine->getVm(), debugHook);
    }
  }
  ~ScopedWatchdog() {
    if (m_engine->m_watchdogEnabled) {
      sq_setnativedebughook(m_engine->getVm(), nullptr);
    }
  }

private:
  ScriptEngine *m_engine;
};

ScriptEngine::ScriptEngine() {}

ScriptEngine::~ScriptEngine() { shutdown(); }

bool ScriptEngine::initialize(vfs::IVfs *vfs, ScriptConfig config) {
  if (m_vm) {
    return true; // Already initialized
  }

  ARCANEE_ASSERT(vfs != nullptr, "VFS pointer must not be null");
  m_vfs = vfs;

  m_vm = sq_open(1024); // Initial stack size
  if (!m_vm) {
    LOG_FATAL("Failed to create Squirrel VM");
    return false;
  }

  // Set foreign pointer to this instance for native callbacks
  sq_setforeignptr(m_vm, this);

  // Set callbacks
  sq_setprintfunc(m_vm, printFunc, errorFunc);
  sq_setcompilererrorhandler(m_vm, compilerErrorFunc);

  // Enable debug info for watchdog/debugging if configured
  sq_enabledebuginfo(m_vm, config.debugInfo ? SQTrue : SQFalse);

  // Set runtime error handler
  sq_newclosure(m_vm, runtimeErrorHandler, 0);
  sq_seterrorhandler(m_vm);

  // Register standard libraries
  registerStandardLibraries();

  // Register ARCANEE API
  registerArcaneeApi();

  // Register global require
  sq_pushroottable(m_vm);
  sq_pushstring(m_vm, "require", -1);
  sq_newclosure(m_vm, require, 0);
  sq_newslot(m_vm, -3, SQFalse);
  sq_pop(m_vm, 1); // Pop root

  LOG_INFO("Squirrel VM initialized");
  return true;
}

void ScriptEngine::shutdown() {
  if (m_vm) {
    // Release cached module objects
    for (auto &pair : m_loadedModules) {
      sq_release(m_vm, &pair.second);
    }
    m_loadedModules.clear();

    sq_close(m_vm);
    m_vm = nullptr;
    LOG_INFO("Squirrel VM shutdown");
  }
}

void ScriptEngine::registerStandardLibraries() {
  sq_pushroottable(m_vm);
  sqstd_register_mathlib(m_vm);
  sqstd_register_stringlib(m_vm);
  sqstd_register_bloblib(m_vm);
  // We do NOT register system/io libs by default to maintain sandbox!
  // sqstd_register_iolib(m_vm);
  // sqstd_register_systemlib(m_vm);
  sq_pop(m_vm, 1);
}

void ScriptEngine::registerArcaneeApi() {
  api::RegisterSysBinding(m_vm);
  api::RegisterFsBinding(m_vm);
  registerGfxBinding(m_vm);   // gfx.* table
  registerAudioBinding(m_vm); // audio.* table
  registerInputBinding(m_vm); // inp.* table
}

void ScriptEngine::setWatchdog(bool enable, f64 timeoutSec) {
  m_watchdogEnabled = enable;
  m_watchdogTimeout = timeoutSec;
}

SQInteger ScriptEngine::require(HSQUIRRELVM vm) {
  ScriptEngine *engine = (ScriptEngine *)sq_getforeignptr(vm);
  ARCANEE_ASSERT(engine != nullptr, "ScriptEngine instance not found");

  const SQChar *path = nullptr;
  if (SQ_FAILED(sq_getstring(vm, 2, &path))) {
    return sq_throwerror(vm, "Invalid argument type");
  }

  std::string resolvedPath = engine->resolvePath(path);
  if (resolvedPath.empty()) {
    return sq_throwerror(vm, "Invalid path");
  }

  // Check cache
  auto it = engine->m_loadedModules.find(resolvedPath);
  if (it != engine->m_loadedModules.end()) {
    sq_pushobject(vm, it->second);
    return 1;
  }

  // Check circular dependency
  for (const auto &stackPath : engine->m_executionStack) {
    if (stackPath == resolvedPath) {
      return sq_throwerror(vm, "Circular dependency detected");
    }
  }

  // Read script
  std::optional<std::string> source = engine->m_vfs->readText(resolvedPath);
  if (!source) {
    return sq_throwerror(vm, "Module not found");
  }

  // Compile
  if (SQ_FAILED(sq_compilebuffer(vm, source->c_str(), source->size(),
                                 resolvedPath.c_str(), SQTrue))) {
    return sq_throwerror(vm, "Module compilation failed");
  }

  // Execute
  sq_pushroottable(vm); // 'this'

  engine->m_executionStack.push_back(resolvedPath);
  SQRESULT res = sq_call(vm, 1, SQTrue, SQTrue);
  engine->m_executionStack.pop_back();

  if (SQ_FAILED(res)) {
    return sq_throwerror(vm, "Module execution failed");
  }

  // Store result in cache
  HSQOBJECT moduleObj;
  sq_getstackobj(vm, -1, &moduleObj);
  sq_addref(vm, &moduleObj);
  engine->m_loadedModules[resolvedPath] = moduleObj;

  return 1; // Return the module object
}

std::string ScriptEngine::resolvePath(const std::string &path) {
  // If path starts with 'cart:/' or similar, it's absolute
  // Otherwise it's relative to current file on stack
  std::string basePath;
  if (!m_executionStack.empty()) {
    basePath = vfs::Path::parent(m_executionStack.back());
  } else {
    basePath = "cart:/";
  }

  // Simple heuristic: if it has a protocol, it's absolute
  if (path.find(":/") != std::string::npos) {
    basePath = "";
  }

  // TODO: Use better path normalization handling .. segments
  // For V0.1 we assume VFS paths are clean or we construct them cleanly
  // This is a minimal implementation relying on VFS to validate final path

  std::string combined = basePath;
  if (!combined.empty() && combined.back() != '/') {
    combined += '/';
  }
  combined += path;

  // Normalize via VFS Path parser if possible, or manual logic
  // Since vfs::Path::parse checks validity, we can try to use it to validate
  // But we need to handle '..' first if we want relative imports like ../foo
  // For now, defer to vfs check, treating 'path' as simple concatenation

  return combined;
}

bool ScriptEngine::executeScript(const std::string &vfsPath) {
  if (!m_vfs) {
    LOG_ERROR("ScriptEngine: VFS not initialized");
    return false;
  }

  // Read script file as text
  std::optional<std::string> scriptSource = m_vfs->readText(vfsPath);
  if (!scriptSource) {
    LOG_ERROR("Failed to read script: %s", vfsPath.c_str());
    return false;
  }

  // Compile
  // Note: sq_compilebuffer expects length in bytes (or chars).
  if (SQ_FAILED(sq_compilebuffer(m_vm, scriptSource->c_str(),
                                 scriptSource->size(), vfsPath.c_str(),
                                 SQTrue))) {
    LOG_ERROR("Compilation failed: %s", vfsPath.c_str());
    return false;
  }

  // Push root table as 'this'
  sq_pushroottable(m_vm);

  // Execute
  m_executionStack.push_back(vfsPath);
  SQRESULT res = sq_call(m_vm, 1, SQFalse, SQTrue);
  m_executionStack.pop_back();

  if (SQ_FAILED(res)) {
    LOG_ERROR("Execution failed: %s", vfsPath.c_str());
    sq_pop(m_vm, 1); // Pop closure
    return false;
  }

  sq_pop(m_vm, 1); // Pop closure
  return true;
}

void ScriptEngine::callInit() {
  if (!m_vm)
    return;

  sq_pushroottable(m_vm);
  sq_pushstring(m_vm, "init", -1);
  if (SQ_SUCCEEDED(sq_get(m_vm, -2))) {
    // Push root table as 'this' for the call
    sq_pushroottable(m_vm);

    ScopedWatchdog watchdog(this);
    SQRESULT res = sq_call(m_vm, 1, SQFalse, SQTrue);
    if (SQ_FAILED(res)) {
      LOG_ERROR("Failed to call init()");
    }
    sq_pop(m_vm, 1); // Pop closure
  } else {
    // init() is optional in code (though spec says mandatory, we can warn)
    LOG_WARN("init() function not found in script");
  }
  sq_pop(m_vm, 1); // Pop root
}

bool ScriptEngine::callUpdate(f64 dt) {
  sq_pushroottable(m_vm);
  sq_pushstring(m_vm, "update", -1);
  if (SQ_FAILED(sq_get(m_vm, -2))) {
    sq_pop(m_vm, 1);
    return false;
  }

  sq_pushroottable(m_vm);
  sq_pushfloat(m_vm, static_cast<SQFloat>(dt));

  ScopedWatchdog watchdog(this);
  if (SQ_FAILED(sq_call(m_vm, 2, SQFalse, SQTrue))) {
    LOG_ERROR("Failed to call update(dt)");
    sq_pop(m_vm, 2);
    return false;
  }

  sq_pop(m_vm, 2);
  return true;
}

bool ScriptEngine::callDraw(f64 alpha) {
  sq_pushroottable(m_vm);
  sq_pushstring(m_vm, "draw", -1);
  if (SQ_FAILED(sq_get(m_vm, -2))) {
    sq_pop(m_vm, 1);
    return false;
  }

  sq_pushroottable(m_vm);
  sq_pushfloat(m_vm, static_cast<SQFloat>(alpha));

  ScopedWatchdog watchdog(this);
  if (SQ_FAILED(sq_call(m_vm, 2, SQFalse, SQTrue))) {
    LOG_ERROR("Failed to call draw(alpha)");
    sq_pop(m_vm, 2);
    return false;
  }

  sq_pop(m_vm, 2);
  return true;
}

} // namespace arcanee::script
