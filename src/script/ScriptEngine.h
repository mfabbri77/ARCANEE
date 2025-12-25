#pragma once

/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file ScriptEngine.h
 */

#include "ScriptDebugger.h" // Added
#include "common/Types.h"
#include "vfs/Vfs.h"
#include <functional>
#include <memory>
#include <squirrel.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace arcanee::script {

// DebugAction defined in ScriptDebugger.h

// DebugBreakpoint defined in BreakpointStore.h

// Local variable for inspection
struct LocalVar {
  std::string name;
  std::string value;
  std::string type;
};

// Stack frame for call stack
struct StackFrame {
  int id;
  std::string name;
  std::string file;
  int line;
};

/**
 * @brief Manages the Squirrel VM instance and cartridge execution lifecycle.
 */
class ScriptEngine {
public:
  friend class ScopedWatchdog; // Allow ScopedWatchdog to access private members
  friend class ScriptDebugger; // Allow ScriptDebugger to access private members
  ScriptEngine();
  ~ScriptEngine();

  struct ScriptConfig {
    bool debugInfo;
    ScriptConfig() : debugInfo(true) {}
  };

  // Prevent copying
  ScriptEngine(const ScriptEngine &) = delete;
  ScriptEngine &operator=(const ScriptEngine &) = delete;

  // Accessors
  vfs::IVfs *getVfs() const { return m_vfs; }
  HSQUIRRELVM getVm() const { return m_vm; }

  /**
   * @brief Initialize the Squirrel VM.
   * @param vfs Pointer to the VFS instance.
   * @param config Configuration options.
   * @return True if initialized successfully, false otherwise.
   */
  bool initialize(vfs::IVfs *vfs, ScriptConfig config = ScriptConfig());

  /**
   * @brief Shuts down the VM and releases resources.
   */
  void shutdown();

  /**
   * @brief Compiles and executes a script from the VFS.
   * @param vfsPath The canonical path to the script (e.g., "cart:/main.nut").
   * @return true on success, false on error.
   */
  bool executeScript(const std::string &vfsPath);

  // Lifecycle hooks
  void callInit();

  /**
   * @brief Calls the update(dt) function.
   */
  bool callUpdate(f64 dt);

  /**
   * @brief Calls the draw(alpha) function.
   */
  bool callDraw(f64 alpha);

  // ========== DEBUGGER API ==========

  /**
   * @brief Enable/disable debugging mode.
   */
  void setDebugEnabled(bool enable);
  bool isDebugEnabled() const;

  /**
   * @brief Access the debugger instance.
   */
  ScriptDebugger *getDebugger() const { return m_debugger.get(); }

  /**
   * @brief Check if VM is paused at breakpoint/step.
   */
  bool isPaused() const { return m_debugger ? m_debugger->isPaused() : false; }

  /**
   * @brief Set debug action (called by DapClient).
   */
  void setDebugAction(DebugAction action);

  /**
   * @brief Breakpoint management.
   */
  void addBreakpoint(const std::string &file, int line);
  void removeBreakpoint(const std::string &file, int line);
  void clearBreakpoints();
  const std::vector<DebugBreakpoint> &getBreakpoints() const;

  /**
   * @brief Inspection (call when paused).
   */
  std::vector<LocalVar> getLocals(int stackLevel = 0);
  std::vector<StackFrame> getCallStack();

  /**
   * @brief Callback when debugger stops (breakpoint or step).
   * Called with line number and source file.
   */
  using DebugStopCallback = ScriptDebugger::StopCallback;
  void setOnDebugStop(DebugStopCallback cb);

  /**
   * @brief Terminate execution immediately.
   */
  void terminate();

private:
  HSQUIRRELVM m_vm = nullptr;
  vfs::IVfs *m_vfs = nullptr;

  // Module system
  std::unordered_map<std::string, HSQOBJECT> m_loadedModules;
  std::vector<std::string> m_executionStack;

  void registerStandardLibraries();
  void registerArcaneeApi();

  // Native binding for require(path)
  static SQInteger require(HSQUIRRELVM vm);

  // Helper to resolve paths relative to current execution context
  std::string resolvePath(const std::string &path);

  // Watchdog
  friend void debugHook(HSQUIRRELVM v, SQInteger type, const SQChar *sourcename,
                        SQInteger line, const SQChar *funcname);
  bool m_watchdogEnabled = false;
  f64 m_watchdogTimeout = 0.5; // Default 500ms
  f64 m_executionStartTime = 0.0;

  // Debugger state
  std::unique_ptr<ScriptDebugger> m_debugger;
  bool m_terminateRequested = false;

  // Helper for value to string conversion
  std::string sqValueToString(HSQUIRRELVM vm, SQInteger idx);

public:
  /**
   * @brief Configure the execution watchdog.
   * @param enable Enable/disable the watchdog.
   * @param timeoutSec Timeout in seconds before execution is aborted.
   */
  void setWatchdog(bool enable, f64 timeoutSec);

  using DebugUpdateCallback = std::function<void()>;
  void setDebugUpdateCallback(DebugUpdateCallback cb) { m_onDebugUpdate = cb; }

private:
  DebugUpdateCallback m_onDebugUpdate;
};

} // namespace arcanee::script
