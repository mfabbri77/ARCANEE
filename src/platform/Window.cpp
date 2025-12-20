#include "Window.h"
#include <iostream>

namespace arcanee::platform {

Window::Window(const Config &config) {
  m_window = SDL_CreateWindow(
      config.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      config.width, config.height,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

  if (!m_window) {
    std::cerr << "Failed to create SDL Window: " << SDL_GetError() << std::endl;
  }
}

Window::~Window() {
  if (m_window) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
}

void Window::getDrawableSize(int &w, int &h) const {
  if (m_window) {
    // Fallback to Window Size for now (SDL < 2.0.14 compatibility)
    // TODO: Use SDL_GetDrawableSize or Vulkan/Metal specific queries for
    // HighDPI
    SDL_GetWindowSize(m_window, &w, &h);
  } else {
    w = 0;
    h = 0;
  }
}

void Window::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      m_shouldClose = true;
    }
  }
}

} // namespace arcanee::platform
