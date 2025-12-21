/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * @file main.cpp
 * @brief ARCANEE entry point.
 *
 * Initializes the platform layer and starts the runtime.
 *
 * @ref specs/Chapter 2 §2.1
 *      "Platform Layer (SDL2): windowing, input, high-resolution timing,
 *       display enumeration, fullscreen control."
 */

#include "app/Runtime.h"
#include "common/Log.h"
#include "common/Version.h"
#include "platform/Platform.h"

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  LOG_INFO("Starting %s v%s", std::string(arcanee::kEngineName).c_str(),
           std::string(arcanee::kEngineVersion).c_str());

  // Initialize platform layer (handles SDL2 init)
  arcanee::platform::PlatformConfig platformConfig;
  platformConfig.enableVideo = true;
  platformConfig.enableAudio = true;
  platformConfig.enableGamepad = true;

  if (!arcanee::platform::Platform::init(platformConfig)) {
    LOG_FATAL("Failed to initialize platform layer");
    return 1;
  }

  // Run the application
  int exitCode = 0;
  {
    arcanee::app::Runtime runtime;
    exitCode = runtime.Run();
  }

  // Shutdown platform layer
  arcanee::platform::Platform::shutdown();
  LOG_INFO("Shutdown complete");

  return exitCode;
}
