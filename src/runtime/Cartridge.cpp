/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file Cartridge.cpp
 */

#include "Cartridge.h"
#include "common/Assert.h"
#include "common/Log.h"
#include "platform/Time.h"

namespace arcanee::runtime {

const char *cartridgeStateToString(CartridgeState state) {
  switch (state) {
  case CartridgeState::Unloaded:
    return "Unloaded";
  case CartridgeState::Loading:
    return "Loading";
  case CartridgeState::Initialized:
    return "Initialized";
  case CartridgeState::Running:
    return "Running";
  case CartridgeState::Paused:
    return "Paused";
  case CartridgeState::Faulted:
    return "Faulted";
  case CartridgeState::Stopped:
    return "Stopped";
  }
  return "Unknown";
}

// CBUF preset dimensions from Chapter 5 §5.3.1
void getCbufDimensions(CartridgeConfig::Display::Aspect aspect,
                       CartridgeConfig::Display::Preset preset, int &outWidth,
                       int &outHeight) {
  using Aspect = CartridgeConfig::Display::Aspect;
  using Preset = CartridgeConfig::Display::Preset;

  if (aspect == Aspect::Ratio16x9 || aspect == Aspect::Any) {
    switch (preset) {
    case Preset::Low:
      outWidth = 480;
      outHeight = 270;
      break;
    case Preset::Medium:
      outWidth = 960;
      outHeight = 540;
      break;
    case Preset::High:
      outWidth = 1920;
      outHeight = 1080;
      break;
    case Preset::Ultra:
      outWidth = 3840;
      outHeight = 2160;
      break;
    }
  } else { // 4:3
    switch (preset) {
    case Preset::Low:
      outWidth = 400;
      outHeight = 300;
      break;
    case Preset::Medium:
      outWidth = 800;
      outHeight = 600;
      break;
    case Preset::High:
      outWidth = 1600;
      outHeight = 1200;
      break;
    case Preset::Ultra:
      outWidth = 3200;
      outHeight = 2400;
      break;
    }
  }
}

// ============================================================================
// Cartridge Implementation
// ============================================================================

Cartridge::Cartridge(vfs::IVfs *vfs, script::ScriptEngine *engine)
    : m_vfs(vfs), m_scriptEngine(engine) {
  ARCANEE_ASSERT(m_vfs != nullptr, "VFS cannot be null");
  ARCANEE_ASSERT(m_scriptEngine != nullptr, "ScriptEngine cannot be null");

  // Enable watchdog for hang protection (500ms limit)
  m_scriptEngine->setWatchdog(true, 0.5);
}

Cartridge::~Cartridge() { unload(); }

bool Cartridge::load(const std::string &fsPath) {
  LOG_INFO("Loading cartridge from: %s", fsPath.c_str());

  if (m_state != CartridgeState::Unloaded) {
    unload(); // Ensure clean state
  }

  transition(CartridgeState::Loading);

  // 1. Initialize VFS for this cartridge
  vfs::VfsConfig vfsConfig;
  vfsConfig.cartridgePath = fsPath;
  vfsConfig.cartridgeId = "unknown"; // TODO: Read ID from manifest properly

  if (!m_vfs->init(vfsConfig)) {
    LOG_ERROR("Failed to mount VFS for cartridge: %s", fsPath.c_str());
    transition(CartridgeState::Faulted);
    return false;
  }

  // TODO: Read manifest.toml here to populate m_config
  // For now we assume defaults and entry point "main.nut"

  // 2. Initialize ScriptEngine with the VFS reference (but don't execute yet)
  script::ScriptEngine::ScriptConfig scriptConfig;
  scriptConfig.debugInfo = true; // Enable debug info by default
  if (!m_scriptEngine->initialize(m_vfs, scriptConfig)) {
    LOG_ERROR("Failed to initialize ScriptEngine");
    transition(CartridgeState::Faulted);
    return false;
  }

  transition(CartridgeState::Initialized);
  LOG_INFO("Cartridge loaded (not running). Call start() to execute.");
  return true;
}

bool Cartridge::start() {
  if (m_state != CartridgeState::Initialized) {
    LOG_ERROR("Cannot start: cartridge not in Initialized state (current: %s)",
              cartridgeStateToString(m_state));
    return false;
  }

  // 1. Compile/Execute entry script
  std::string entryPath = "cart:/" + m_config.entry;
  LOG_INFO("Executing entry script: %s", entryPath.c_str());

  if (!m_scriptEngine->executeScript(entryPath)) {
    LOG_ERROR("Failed to execute entry script");
    transition(CartridgeState::Faulted);
    return false;
  }

  // 2. Call init() if exists
  m_scriptEngine->callInit();

  transition(CartridgeState::Running);
  LOG_INFO("Cartridge started and running");
  return true;
}

void Cartridge::unload() {
  if (m_state == CartridgeState::Unloaded)
    return;

  LOG_INFO("Unloading cartridge");

  // Stop VM
  m_scriptEngine->shutdown();

  // Unmount VFS
  m_vfs->shutdown();

  transition(CartridgeState::Unloaded);
}

void Cartridge::update(double dt) {
  if (m_state == CartridgeState::Running) {
    // TODO wrap in try-catch if using C++ exceptions for script errors
    double start = platform::Time::now();
    m_scriptEngine->callUpdate(dt);
    double elapsed = platform::Time::now() - start;
    if (elapsed > 0.016) { // 16ms budget
      LOG_WARN("Performance Warning: update() took %.2fms (Budget: 16.00ms)",
               elapsed * 1000.0);
    }
  }
}

void Cartridge::draw(double alpha) {
  // Never call script draw when VM is suspended (debugger paused)
  if ((m_state == CartridgeState::Running ||
       m_state == CartridgeState::Paused) &&
      !m_scriptEngine->isPaused()) {
    m_scriptEngine->callDraw(alpha);
  }
}

void Cartridge::transition(CartridgeState newState) {
  if (m_state == newState)
    return;

  LOG_INFO("Cartridge State: %s -> %s", cartridgeStateToString(m_state),
           cartridgeStateToString(newState));
  m_state = newState;
}

} // namespace arcanee::runtime
