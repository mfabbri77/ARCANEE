#pragma once

#include "platform/Window.h"
#include "render/RenderDevice.h"
#include <SDL2/SDL_events.h>
#include <memory>

namespace arcanee::app {

class Workbench {
public:
  Workbench();
  ~Workbench();

  // Prevent copying
  Workbench(const Workbench &) = delete;
  Workbench &operator=(const Workbench &) = delete;

  bool initialize(render::RenderDevice *device, platform::Window *window);
  void shutdown();

  void update(double dt);
  void render(render::RenderDevice *device);
  bool handleInput(const SDL_Event *event);

  void toggle() { m_visible = !m_visible; }
  bool isVisible() const { return m_visible; }

private:
  bool m_visible = false;

  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace arcanee::app
