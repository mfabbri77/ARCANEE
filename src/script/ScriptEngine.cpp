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
#include "BindingHelpers.h"
#include "api/AudioBinding.h"
#include "api/FsBinding.h"
#include "api/GfxBinding.h"
#include "api/InputBinding.h"
#include "api/SysBinding.h"
#include "common/Assert.h"
#include "common/Log.h"
#include "platform/Time.h"
#include "vfs/Vfs.h"
#include <algorithm>
#include <cstdarg>
#include <optional>
#include <sqstdaux.h>
#include <sqstdblob.h>
#include <sqstdmath.h>
#include <sqstdstring.h>

namespace arcanee::script {

namespace {

// Error print callback (routes to LOG_ERROR)
// Output print callback (routes to LOG_INFO)
[[maybe_unused]]
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

[[maybe_unused]]
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

// Debug hook moved to ScriptDebugger

ScriptEngine::ScriptEngine() {
  m_debugger = std::make_unique<ScriptDebugger>(this);
}

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

  // Set print/error callbacks
  sq_setprintfunc(m_vm, printFunc, errorFunc);

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

  // Re-attach debug hook if debugging was enabled (persisted across reloads)
  // Attach debugger if configured
  m_debugger->attach(m_vm);

  // instead of legare l'enable a debugInfo, lega a "modalità debug attiva"
  if (m_debugger->isEnabled()) {
    m_debugger->setEnabled(true); // reinstall hook sul nuovo vm
  } else if (config.debugInfo) {
    // Logic kept: if config mandates it (e.g. first load), set enabled?
    // Actually friend says "invece di legare l'enable a debugInfo"
    // But we might want initial state. Let's keep existing logic as fallback or
    // just use m_debugger->isEnabled() if it was already set. But wait,
    // m_debugger persists across initialize() because it is a member of
    // ScriptEngine. So checking isEnabled() is correct. However, if it's the
    // *first* time, isEnabled is false. DapClient calls setDebugEnabled(true).
    // So if we reload, isEnabled is true.
    // What about config.debugInfo? It enables compile-time debug info.
    // It was: if (config.debugInfo) m_debugger->setEnabled(true);
    // We should probably respect config.debugInfo for the *initial* enablement
    // if it's meant to force it. But typically DebugInfo is just for
    // compilation. Hook enablement is separate. Let's rely on
    // m_debugger->isEnabled().
  }

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

// ========== DEBUGGER IMPLEMENTATION ==========

void ScriptEngine::setDebugEnabled(bool enable) {
  if (m_debugger) {
    m_debugger->setEnabled(enable);
  }
}

bool ScriptEngine::isDebugEnabled() const {
  return m_debugger && m_debugger->isEnabled();
}

void ScriptEngine::setOnDebugStop(DebugStopCallback cb) {
  if (m_debugger)
    m_debugger->setStopCallback(std::move(cb));
}

void ScriptEngine::setDebugUIPump(DebugUIPumpCallback cb) {
  if (m_debugger)
    m_debugger->setUIPumpCallback(std::move(cb));
}

void ScriptEngine::setDebugShouldExit(DebugShouldExitCallback cb) {
  if (m_debugger)
    m_debugger->setShouldExitCallback(std::move(cb));
}

void ScriptEngine::setDebugAction(DebugAction action) {
  if (m_debugger) {
    // Logic for resume moved to debugger
    if (action == DebugAction::Continue) {
      m_debugger->resume();
    } else {
      m_debugger->setAction(action);
      // If paused, we might need to resume to step?
      // If we are setting an action, we probably want to resume if paused.
      m_debugger->resume();
    }
  }
}

void ScriptEngine::addBreakpoint(const std::string &file, int line) {
  LOG_INFO("addBreakpoint engine=%p vm=%p debugger=%p file=%s line=%d", this,
           m_vm, m_debugger.get(), file.c_str(), line);
  if (m_debugger) {
    m_debugger->getBreakpoints().add(file, line);
  }
}

void ScriptEngine::removeBreakpoint(const std::string &file, int line) {
  if (m_debugger) {
    m_debugger->getBreakpoints().remove(file, line);
  }
}

void ScriptEngine::clearBreakpoints() {
  if (m_debugger) {
    m_debugger->getBreakpoints().clear();
  }
}

std::vector<LocalVar> ScriptEngine::getLocals(int stackLevel) {
  std::vector<LocalVar> result;
  if (!m_vm)
    return result;

  SQUnsignedInteger idx = 0;
  const SQChar *name;
  while ((name = sq_getlocal(m_vm, stackLevel, idx)) != nullptr) {
    LocalVar var;
    var.name = name;
    var.type = "unknown";

    SQObjectType t = sq_gettype(m_vm, -1);
    switch (t) {
    case OT_NULL:
      var.type = "null";
      var.value = "null";
      break;
    case OT_INTEGER: {
      SQInteger val;
      sq_getinteger(m_vm, -1, &val);
      var.type = "integer";
      var.value = std::to_string(val);
      break;
    }
    case OT_FLOAT: {
      SQFloat val;
      sq_getfloat(m_vm, -1, &val);
      var.type = "float";
      var.value = std::to_string(val);
      break;
    }
    case OT_BOOL: {
      SQBool val;
      sq_getbool(m_vm, -1, &val);
      var.type = "bool";
      var.value = val ? "true" : "false";
      break;
    }
    case OT_STRING: {
      const SQChar *str;
      sq_getstring(m_vm, -1, &str);
      var.type = "string";
      var.value = std::string("\"") + str + "\"";
      break;
    }
    case OT_TABLE:
      var.type = "table";
      var.value = "{...}";
      break;
    case OT_ARRAY:
      var.type = "array";
      var.value = "[...]";
      break;
    case OT_CLOSURE:
      var.type = "function";
      var.value = "<function>";
      break;
    case OT_NATIVECLOSURE:
      var.type = "native";
      var.value = "<native>";
      break;
    case OT_CLASS:
      var.type = "class";
      var.value = "<class>";
      break;
    case OT_INSTANCE:
      var.type = "instance";
      var.value = "<instance>";
      break;
    default:
      var.type = "unknown";
      var.value = "?";
      break;
    }

    sq_pop(m_vm, 1); // Pop the local value
    result.push_back(var);
    idx++;
  }
  return result;
}

std::vector<StackFrame> ScriptEngine::getCallStack() {
  std::vector<StackFrame> result;
  if (!m_vm)
    return result;

  SQInteger level = 0;
  SQStackInfos si;
  while (SQ_SUCCEEDED(sq_stackinfos(m_vm, level, &si))) {
    StackFrame frame;
    frame.id = static_cast<int>(level);
    frame.name = si.funcname ? si.funcname : "<unknown>";
    frame.file = si.source ? si.source : "<unknown>";
    frame.line = static_cast<int>(si.line);
    result.push_back(frame);
    level++;
  }
  return result;
}

std::string ScriptEngine::sqValueToString(HSQUIRRELVM vm, SQInteger idx) {
  SQObjectType t = sq_gettype(vm, idx);
  switch (t) {
  case OT_NULL:
    return "null";
  case OT_INTEGER: {
    SQInteger val;
    sq_getinteger(vm, idx, &val);
    return std::to_string(val);
  }
  case OT_FLOAT: {
    SQFloat val;
    sq_getfloat(vm, idx, &val);
    return std::to_string(val);
  }
  case OT_BOOL: {
    SQBool val;
    sq_getbool(vm, idx, &val);
    return val ? "true" : "false";
  }
  case OT_STRING: {
    const SQChar *str;
    sq_getstring(vm, idx, &str);
    return std::string("\"") + str + "\"";
  }
  case OT_TABLE:
    return "{...}";
  case OT_ARRAY:
    return "[...]";
  case OT_CLOSURE:
    return "<function>";
  case OT_NATIVECLOSURE:
    return "<native>";
  default:
    return "?";
  }
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

  // Set execution start time for watchdog
  m_executionStartTime = platform::Time::now();

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

const std::vector<DebugBreakpoint> &ScriptEngine::getBreakpoints() const {
  static const std::vector<DebugBreakpoint> empty;
  if (m_debugger) {
    return m_debugger->getBreakpoints().getAll();
  }
  return empty;
}

void ScriptEngine::callInit() {
  if (!m_vm)
    return;

  // Set execution start time for watchdog
  m_executionStartTime = platform::Time::now();

  sq_pushroottable(m_vm);
  sq_pushstring(m_vm, "init", -1);
  if (SQ_SUCCEEDED(sq_get(m_vm, -2))) {
    // Push root table as 'this' for the call
    sq_pushroottable(m_vm);

    // ScopedWatchdog removed, logic handled by debugger hook if active
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
  // If paused, do nothing (gameplay suspended)
  if (m_debugger && m_debugger->isPaused())
    return true;

  // Set execution start time for watchdog
  m_executionStartTime = platform::Time::now();

  sq_pushroottable(m_vm);
  sq_pushstring(m_vm, "update", -1);
  if (SQ_FAILED(sq_get(m_vm, -2))) {
    sq_pop(m_vm, 1);
    return false;
  }

  sq_pushroottable(m_vm);
  sq_pushfloat(m_vm, static_cast<SQFloat>(dt));

  m_terminateRequested = false; // Reset flag at start of frame

  // NOTE: We don't use ScopedWatchdog here explicitly, ScopedWatchdog logic
  // pending move. For now we assume Debugger hook handles it or we'll add it
  // back to Debugger.

  SQRESULT res = sq_call(m_vm, 2, SQFalse, SQTrue);

  // Handle Suspension
  if (sq_getvmstate(m_vm) == SQ_VMSTATE_SUSPENDED) {
    // Suspended! Do NOT pop anything.
    // Also notify anyone?
    return true;
  }

  if (SQ_FAILED(res)) {
    LOG_ERROR("Failed to call update(dt)");
    sq_pop(m_vm, 2);
    return false;
  }

  sq_pop(m_vm, 2);
  return true;
}

bool ScriptEngine::callDraw(f64 alpha) {
  // Set execution start time for watchdog
  m_executionStartTime = platform::Time::now();

  sq_pushroottable(m_vm);
  sq_pushstring(m_vm, "draw", -1);
  if (SQ_FAILED(sq_get(m_vm, -2))) {
    sq_pop(m_vm, 1);
    return false;
  }

  sq_pushroottable(m_vm);
  sq_pushfloat(m_vm, static_cast<SQFloat>(alpha));

  // No watchdog on draw directly, or share logic?

  SQRESULT res = sq_call(m_vm, 2, SQFalse, SQTrue);

  if (sq_getvmstate(m_vm) == SQ_VMSTATE_SUSPENDED) {
    return true;
  }

  if (SQ_FAILED(res)) {
    LOG_ERROR("Failed to call draw(alpha)");
    sq_pop(m_vm, 2);
    return false;
  }

  sq_pop(m_vm, 2);
  return true;
}

void ScriptEngine::terminate() {
  if (m_vm) {
    m_terminateRequested = true;
    // We should resume if paused to allow termination exception to throw?
    if (m_debugger && m_debugger->isPaused()) {
      m_debugger->resume();
    }
    // ensure hook is engaged (debugger does this if enabled)
  }
}

} // namespace arcanee::script
