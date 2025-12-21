#pragma once

#include "InputState.h"
#include "common/Types.h"
#include "platform/Window.h"
#include <SDL2/SDL.h>
#include <memory>

namespace arcanee::input {

class InputManager;

// Global Access
void setInputManager(InputManager *manager);
InputManager *getInputManager();

class InputManager {
public:
  InputManager();
  ~InputManager();

  bool initialize(platform::Window *window);
  void update(); // Pump events and update state

  // Access to snapshots
  const InputSnapshot &getCurrentSnapshot() const { return m_currentSnapshot; }
  const InputSnapshot &getPreviousSnapshot() const {
    return m_previousSnapshot;
  }

  // Helpers for API bindings
  bool isKeyDown(int scancode) const;
  bool isKeyPressed(int scancode) const;  // Down this frame, Up last
  bool isKeyReleased(int scancode) const; // Up this frame, Down last

  bool isMouseButtonDown(int btn) const;
  bool isMouseButtonPressed(int btn) const;
  bool isMouseButtonReleased(int btn) const;

  // Mouse pos in Console Space (depends on Viewport)
  // Note: SDL events usually give Window coords.
  // We need to map them to Console Space.
  // This might require passing the current Viewport to update() or having a
  // setter. For now, let's store raw window coords in internal state and map
  // them during update() if we have access to viewport, OR map them when
  // requested? Spec 9.5.1: "inp.mousePos() returns (x,y) in console
  // coordinates" Spec 9.2.1: "Input state is then stored... per-tick snapshots"
  // So the snapshot MUST contain the Console Space coordinates?
  // Yes, because "All inp.* queries... MUST read from the Tick Input Snapshot".
  // So we need to calculate console coords during update().

  // We'll need to know the current Viewport (set by Render/Present logic).
  // Ideally, Runtime sets this on InputManager before update().
  void setViewport(int x, int y, int w, int h, int canvasW, int canvasH);

  // Gamepad
  int getGamepadCount() const;
  bool isGamepadButtonDown(int padIdx, int btn) const;
  bool isGamepadButtonPressed(int padIdx, int btn) const;
  bool isGamepadButtonReleased(int padIdx, int btn) const;
  f32 getGamepadAxis(int padIdx, int axis) const;

private:
  void processEvent(const SDL_Event &event);
  void updateMouseCoordinates();

  // Internal state tracking (live from SDL events)
  InputSnapshot m_liveState;

  // Snapshot history
  InputSnapshot m_currentSnapshot;
  InputSnapshot m_previousSnapshot;

  platform::Window *m_window = nullptr;

  // Viewport for coordinate mapping
  struct {
    int x = 0, y = 0, w = 1, h = 1;
    int canvasW = 128, canvasH = 128; // CBUF dims
  } m_viewport;

  // Gamepad handles (SDL_GameController*)
  std::array<SDL_GameController *, kMaxGamepads> m_controllers{};
};

} // namespace arcanee::input
