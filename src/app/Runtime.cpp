#include "Runtime.h"
#include "common/Log.h"
#include "runtime/Cartridge.h"
#include <SDL.h>
#include <algorithm>

namespace arcanee::app {

// Normative Constants
constexpr double kTickHz = 60.0;
constexpr double kDtFixed = 1.0 / kTickHz;
constexpr int kMaxUpdatesPerFrame = 4;

Runtime::Runtime() { InitSubsystems(); }

Runtime::~Runtime() { ShutdownSubsystems(); }

void Runtime::InitSubsystems() {
  // 1. Platform (Window)
  platform::Window::Config winConfig;
  winConfig.title = "ARCANEE v0.1";
  winConfig.width = 1280;
  winConfig.height = 720;

  m_window = std::make_unique<platform::Window>(winConfig);

  if (!m_window->isOpen()) {
    LOG_ERROR("Runtime: Failed to create window");
    m_running = false;
  }
}

void Runtime::ShutdownSubsystems() {
  // Reverse order destruction happens automatically via unique_ptr
  m_window.reset();
}

int Runtime::Run() {
  if (!m_running)
    return 1;

  LOG_INFO("Runtime: Starting Main Loop (Fixed Timestep: %.0f Hz)", kTickHz);

  double accumulator = 0.0;
  uint64_t perfFreq = SDL_GetPerformanceFrequency();
  uint64_t previousTime = SDL_GetPerformanceCounter();

  while (m_running && !m_window->shouldClose()) {
    // 1. Timing
    uint64_t currentTime = SDL_GetPerformanceCounter();
    double frameTime =
        static_cast<double>(currentTime - previousTime) / perfFreq;
    previousTime = currentTime;

    // Clamp large frame times (e.g. debugging)
    if (frameTime > 0.25)
      frameTime = 0.25;

    accumulator += frameTime;

    // 2. Event Pump
    m_window->pollEvents();
    if (m_window->shouldClose()) {
      m_running = false;
      break;
    }

    // 3. Fixed Updates
    int updateCount = 0;
    while (accumulator >= kDtFixed && updateCount < kMaxUpdatesPerFrame) {
      Update(kDtFixed);
      accumulator -= kDtFixed;
      updateCount++;
    }

    if (accumulator > kDtFixed * kMaxUpdatesPerFrame) {
      // Spiral of death prevention
      // std::cout << "Warning: Frame budget exceeded, dropping time." <<
      // std::endl;
      accumulator = 0.0;
    }

    // 4. Draw
    double alpha = accumulator / kDtFixed;
    alpha = std::clamp(alpha, 0.0, 1.0);
    Draw(alpha);

    // Throttle to avoid 100% CPU usage on empty loop
    // In reality, VSync or Renderer waiting will handle this
    // For now, small sleep if frame was super fast
    if (frameTime < 0.001) {
      SDL_Delay(1);
    }
  }

  return 0;
}

void Runtime::Update(double dt) {
  (void)dt;
  // TODO: Cartridge Update
}

void Runtime::Draw(double alpha) {
  (void)alpha;
  // TODO: Cartridge Draw / Render
}

} // namespace arcanee::app
