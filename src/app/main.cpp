#include "common/Version.h"
#include <SDL.h>
#include <iostream>

#include "app/Runtime.h"

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  std::cout << "Starting " << arcanee::kEngineName << " v"
            << arcanee::kEngineVersion << std::endl;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
    std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
    return 1;
  }
  std::cout << "SDL2 Initialized successfully." << std::endl;

  {
    arcanee::app::Runtime runtime;
    runtime.Run();
  }

  SDL_Quit();
  return 0;
}
