#pragma once

#include "platform/Window.h"
#include <memory>

namespace arcanee::app {

class Runtime {
public:
  Runtime();
  ~Runtime();

  // Runs the main loop until exit
  int Run();

private:
  void InitSubsystems();
  void ShutdownSubsystems();

  // Fixed timestep methods
  void Update(double dt);
  void Draw(double alpha);

  // Subsystems
  std::unique_ptr<platform::Window> m_window;

  // Timing state
  bool m_running = true;
};

} // namespace arcanee::app
