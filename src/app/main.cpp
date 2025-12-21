#include "common/Log.h"
#include "common/Version.h"
#include <SDL.h>

#include "app/Runtime.h"

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  LOG_INFO("Starting %s v%s", std::string(arcanee::kEngineName).c_str(),
           std::string(arcanee::kEngineVersion).c_str());

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
    LOG_FATAL("SDL_Init Error: %s", SDL_GetError());
    return 1;
  }
  LOG_INFO("SDL2 Initialized successfully");

  {
    arcanee::app::Runtime runtime;
    runtime.Run();
  }

  SDL_Quit();
  LOG_INFO("Shutdown complete");
  return 0;
}
