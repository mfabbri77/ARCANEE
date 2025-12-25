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

// Debug and watchdog hook
void debugHook(HSQUIRRELVM v, SQInteger type, const SQChar *sourcename,
               SQInteger line, const SQChar *funcname) {
  ScriptEngine *engine = (ScriptEngine *)sq_getforeignptr(v);
  if (!engine)
    return;

  // Debug logging to verify hook is active
  fprintf(stderr, "DEBUG_HOOK: Type=%c Line=%lld Source=%s\n", (char)type,
          (long long)line, sourcename ? sourcename : "null");
  // LOG_INFO("DEBUG_HOOK: Type=%c Line=%lld Source=%s", (char)type, line,
  // sourcename ? sourcename : "null");

  // Watchdog check (timeout)
  if (engine->m_watchdogEnabled) {
    double elapsed = platform::Time::now() - engine->m_executionStartTime;
    if (elapsed > engine->m_watchdogTimeout) {
      sq_throwerror(v, "Watchdog timeout: Execution time limit exceeded");
      return;
    }
  }

  // Handle explicit termination request
  if (engine->m_terminateRequested) {
    engine->m_terminateRequested = false;
    sq_throwerror(v, "Execution terminated by user");
    return;
  }

  // Debugger check - only on line events (type 'l')
  if (engine->m_debugEnabled && type == 'l') {
    bool shouldStop = false;
    std::string stopReason;
    std::string file = sourcename ? sourcename : "";

    // Debug logging for breakpoint state
    // Only log occasionally or if size > 0 to diagnose
    // if (engine->m_breakpoints.size() > 0) {
    //    fprintf(stderr, "HOOK: Line %lld. BPs: %zu\n", (long long)line,
    //    engine->m_breakpoints.size()); for(auto& b : engine->m_breakpoints) {
    //        fprintf(stderr, "  - BP: %s:%d (En: %d)\n", b.file.c_str(),
    //        b.line, b.enabled);
    //    }
    // }

    // Check breakpoints
    for (const auto &bp : engine->m_breakpoints) {
      if (bp.enabled && bp.line == static_cast<int>(line)) {
        // Match Logic:
        // 1. Exact match
        // 2. Contains match (for partial paths)
        // 3. Filename match (Fall-back for VFS vs Absolute path mismatch)

        bool match = (file == bp.file);
        if (!match && (file.find(bp.file) != std::string::npos ||
                       bp.file.find(file) != std::string::npos)) {
          match = true;
        }

        if (!match) {
          // Extract filenames
          auto sep1 = file.find_last_of("/\\");
          std::string fn1 =
              (sep1 == std::string::npos) ? file : file.substr(sep1 + 1);

          auto sep2 = bp.file.find_last_of("/\\");
          std::string fn2 =
              (sep2 == std::string::npos) ? bp.file : bp.file.substr(sep2 + 1);

          if (fn1 == fn2) {
            match = true;
          }
        }

        fprintf(stderr, "BP CHECK: Line %d. VM: '%s'. BP: '%s'. Match: %s\n",
                (int)line, file.c_str(), bp.file.c_str(), match ? "YES" : "NO");

        if (match) {
          shouldStop = true;
          stopReason = "breakpoint";
          break;
        }
      }
    }

    // Check step action
    if (!shouldStop && engine->m_debugAction != DebugAction::None) {
      switch (engine->m_debugAction) {
      case DebugAction::StepIn:
        shouldStop = true;
        stopReason = "step";
        break;
      case DebugAction::StepOver: {
        // Stop if same or lower depth
        SQInteger depth = 0;
        SQStackInfos si;
        while (SQ_SUCCEEDED(sq_stackinfos(v, depth, &si)))
          depth++;
        if (static_cast<int>(depth) <= engine->m_stepStartDepth) {
          shouldStop = true;
          stopReason = "step";
        }
        break;
      }
      case DebugAction::StepOut: {
        // Stop if lower depth (returned from function)
        SQInteger depth = 0;
        SQStackInfos si;
        while (SQ_SUCCEEDED(sq_stackinfos(v, depth, &si)))
          depth++;
        if (static_cast<int>(depth) < engine->m_stepStartDepth) {
          shouldStop = true;
          stopReason = "step";
        }
        break;
      }
      case DebugAction::Continue:
        // Only stop at breakpoints (already checked above)
        break;
      default:
        break;
      }
    }

    if (shouldStop) {
      engine->m_debugAction = DebugAction::None;
      engine->m_debugPaused = true;

      // Notify callback
      if (engine->m_onDebugStop) {
        engine->m_onDebugStop(static_cast<int>(line), file, stopReason);
      }

      // Note: In a real implementation, we would need to block here
      // until the debugger signals to continue. For cooperative debugging,
      // we set m_debugPaused and the main loop checks this flag.
      // UPDATE: We now block here to preserve stack state!
      while (engine->m_debugPaused && !engine->m_terminateRequested) {
        if (engine->m_onDebugUpdate) {
          engine->m_onDebugUpdate();
        } else {
          // No UI loop available, just sleep (fallback)
          // Note: using sleep implies we need <thread> and <chrono>
          // Ideally we shouldn't hit this path if Workbench is attached.
        }
      }
    }
  }
}

class ScopedWatchdog {
public:
  explicit ScopedWatchdog(ScriptEngine *engine) : m_engine(engine) {
    if (m_engine->m_watchdogEnabled && !m_engine->m_debugEnabled) {
      m_engine->m_executionStartTime = platform::Time::now();
      // Hook on lines to catch infinite loops (while(true))
      sq_setnativedebughook(m_engine->getVm(), debugHook);
    }
  }
  ~ScopedWatchdog() {
    if (m_engine->m_watchdogEnabled && !m_engine->m_debugEnabled) {
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
  // Re-attach debug hook if debugging was enabled (persisted across reloads)
  if (m_debugEnabled) {
    fprintf(stderr, "DEBUG: ScriptEngine::initialize: m_debugEnabled is TRUE. "
                    "Re-attaching hook.\n");
    sq_setnativedebughook(m_vm, debugHook);
  } else {
    fprintf(stderr, "DEBUG: ScriptEngine::initialize: m_debugEnabled is FALSE. "
                    "No hook attached.\n");
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
  fprintf(stderr, "DEBUG: ScriptEngine::setDebugEnabled(%s). VM=%p\n",
          enable ? "true" : "false", (void *)m_vm);
  // LOG_INFO("ScriptEngine::setDebugEnabled(%s). VM=%p", enable ? "true" :
  // "false", m_vm);
  m_debugEnabled = enable;
  if (m_vm) {
    if (enable) {
      sq_setnativedebughook(m_vm, debugHook);
    } else {
      sq_setnativedebughook(m_vm, nullptr);
    }
  }
}

void ScriptEngine::setDebugAction(DebugAction action) {
  m_debugAction = action;
  if (action != DebugAction::None) {
    // Record current stack depth for step over/out
    if (m_vm) {
      SQInteger depth = 0;
      SQStackInfos si;
      while (SQ_SUCCEEDED(sq_stackinfos(m_vm, depth, &si)))
        depth++;
      m_stepStartDepth = static_cast<int>(depth);
    }
    m_debugPaused = false; // Resume execution
  }
}

void ScriptEngine::addBreakpoint(const std::string &file, int line) {
  fprintf(stderr, "ScriptEngine::addBreakpoint %s:%d\n", file.c_str(), line);
  // Check if already exists
  for (auto &bp : m_breakpoints) {
    if (bp.file == file && bp.line == line) {
      bp.enabled = true;
      return;
    }
  }
  m_breakpoints.push_back({file, line, true});
}

void ScriptEngine::removeBreakpoint(const std::string &file, int line) {
  fprintf(stderr, "ScriptEngine::removeBreakpoint %s:%d\n", file.c_str(), line);
  m_breakpoints.erase(std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
                                     [&](const DebugBreakpoint &bp) {
                                       return bp.file == file &&
                                              bp.line == line;
                                     }),
                      m_breakpoints.end());
}

void ScriptEngine::clearBreakpoints() { m_breakpoints.clear(); }

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

  m_terminateRequested = false; // Reset flag at start of frame
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

void ScriptEngine::terminate() {
  if (m_vm) {
    m_terminateRequested = true;
    m_debugPaused = false; // Force resume so we can hit the hook
    // Ensure debug hook is enabled so we catch the next instruction
    sq_setnativedebughook(m_vm, debugHook);
  }
}

} // namespace arcanee::script
