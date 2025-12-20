#pragma once

#include <SDL.h>
#include <string>

namespace arcanee::platform {

class Window {
public:
  struct Config {
    std::string title = "ARCANEE";
    int width = 1280;
    int height = 720;
  };

  Window(const Config &config);
  ~Window();

  // Deleted copy/move for now for simplicity
  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  bool isOpen() const { return m_window != nullptr; }
  void pollEvents();
  bool shouldClose() const { return m_shouldClose; }

  // Accessors
  SDL_Window *getNativeHandle() const { return m_window; }
  void getDrawableSize(int &w, int &h) const;

private:
  SDL_Window *m_window = nullptr;
  bool m_shouldClose = false;
};

} // namespace arcanee::platform
