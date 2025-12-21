#include "InputManager.h"
#include "common/Log.h"
#include <cstring> // for memcpy, memset

namespace arcanee::input {

static InputManager *g_inputManager = nullptr;

void setInputManager(InputManager *manager) { g_inputManager = manager; }

InputManager *getInputManager() { return g_inputManager; }

InputManager::InputManager()
    : m_liveState{}, m_currentSnapshot{}, m_previousSnapshot{} {}

InputManager::~InputManager() {
  for (auto *ctrl : m_controllers) {
    if (ctrl)
      SDL_GameControllerClose(ctrl);
  }
}

bool InputManager::initialize(platform::Window *window) {
  m_window = window;

  if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
    LOG_ERROR("Failed to init SDL GameController: {}", SDL_GetError());
    // Not fatal, but good to know
  }

  // Open existing controllers
  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    if (SDL_IsGameController(i)) {
      if (i < kMaxGamepads) {
        m_controllers[i] = SDL_GameControllerOpen(i);
        if (m_controllers[i]) {
          m_liveState.gamepads[i].connected = true;
          LOG_INFO("Gamepad connected: {}",
                   SDL_GameControllerName(m_controllers[i]));
        }
      }
    }
  }

  return true;
}

void InputManager::setViewport(int x, int y, int w, int h, int canvasW,
                               int canvasH) {
  m_viewport.x = x;
  m_viewport.y = y;
  m_viewport.w = w;
  m_viewport.h = h;
  m_viewport.canvasW = canvasW;
  m_viewport.canvasH = canvasH;
}

void InputManager::update() {
  // 1. Cycle snapshots
  m_previousSnapshot = m_currentSnapshot;

  // 2. Pump SDL events and update m_liveState
  // Note: SDL_PumpEvents() is usually called by SDL_PollEvent()
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    processEvent(event);
  }

  // 3. Update mouse coordinates in live state based on current viewport
  // We do this every frame to ensure smoothly updated coords even if no mouse
  // event
  updateMouseCoordinates();

  // 4. Freeze live state into current snapshot
  m_currentSnapshot = m_liveState;

  // Reset per-tick accumulators in live state (e.g. wheel)
  m_liveState.mouse.wheelX = 0.0f;
  m_liveState.mouse.wheelY = 0.0f;
}

void InputManager::processEvent(const SDL_Event &event) {
  // Handle ImGui / Workbench capture here later?
  // For now, raw capture.

  switch (event.type) {
  case SDL_KEYDOWN:
    if (event.key.keysym.scancode < 512) {
      m_liveState.keys[event.key.keysym.scancode] = 1;
    }
    break;
  case SDL_KEYUP:
    if (event.key.keysym.scancode < 512) {
      m_liveState.keys[event.key.keysym.scancode] = 0;
    }
    break;
  case SDL_MOUSEBUTTONDOWN:
    if (event.button.button == SDL_BUTTON_LEFT)
      m_liveState.mouse.buttons |= (1 << kMouseButtonLeft);
    if (event.button.button == SDL_BUTTON_MIDDLE)
      m_liveState.mouse.buttons |= (1 << kMouseButtonMiddle);
    if (event.button.button == SDL_BUTTON_RIGHT)
      m_liveState.mouse.buttons |= (1 << kMouseButtonRight);
    if (event.button.button == SDL_BUTTON_X1)
      m_liveState.mouse.buttons |= (1 << kMouseButtonX1);
    if (event.button.button == SDL_BUTTON_X2)
      m_liveState.mouse.buttons |= (1 << kMouseButtonX2);
    break;
  case SDL_MOUSEBUTTONUP:
    if (event.button.button == SDL_BUTTON_LEFT)
      m_liveState.mouse.buttons &= ~(1 << kMouseButtonLeft);
    if (event.button.button == SDL_BUTTON_MIDDLE)
      m_liveState.mouse.buttons &= ~(1 << kMouseButtonMiddle);
    if (event.button.button == SDL_BUTTON_RIGHT)
      m_liveState.mouse.buttons &= ~(1 << kMouseButtonRight);
    if (event.button.button == SDL_BUTTON_X1)
      m_liveState.mouse.buttons &= ~(1 << kMouseButtonX1);
    if (event.button.button == SDL_BUTTON_X2)
      m_liveState.mouse.buttons &= ~(1 << kMouseButtonX2);
    break;
  case SDL_MOUSEWHEEL:
    m_liveState.mouse.wheelX += event.wheel.preciseX;
    m_liveState.mouse.wheelY += event.wheel.preciseY;
    break;

  // Gamepad hotplug
  case SDL_CONTROLLERDEVICEADDED: {
    int idx = event.cdevice.which;
    if (idx < kMaxGamepads && !m_controllers[idx]) {
      m_controllers[idx] = SDL_GameControllerOpen(idx);
      m_liveState.gamepads[idx].connected = true;
      LOG_INFO("Gamepad connected: {}", idx);
    }
    break;
  }
  case SDL_CONTROLLERDEVICEREMOVED: {
    // which is instance_id, need to find index
    // Simpler: iterate controllers
    int instanceId = event.cdevice.which;
    for (int i = 0; i < kMaxGamepads; ++i) {
      if (m_controllers[i] &&
          SDL_JoystickInstanceID(
              SDL_GameControllerGetJoystick(m_controllers[i])) == instanceId) {
        SDL_GameControllerClose(m_controllers[i]);
        m_controllers[i] = nullptr;
        m_liveState.gamepads[i].connected = false;
        LOG_INFO("Gamepad disconnected: {}", i);
        break;
      }
    }
    break;
  }

    // Gamepad buttons/axes
    // We can also poll SDL_GameControllerGetButton/Axis explicitly in update()
    // instead of tracking events, which might be simpler for snapshots.
    // Let's rely on polling in updateMouseCoordinates-like function for
    // Gamepads too? Mixed approach: key/mouse buttons are event driven
    // (robustness), axes are generally polled. Actually, for snapshots, polling
    // state at start of frame is often cleanest. But we are in processEvent
    // loop. Let's do polling for Gamepad at end of update() for simplicity to
    // sync with SDL state. Keys/Mouse buttons are better event driven to not
    // miss sub-frame clicks? Spec says: "Input is collected ... and converted
    // into per-tick snapshots". "Pressed is true for the first tick in which
    // key becomes down". If we poll, we match SDL internal state. SDL updates
    // its internal state during PumpEvents. So polling SDL_GetKeyboardState etc
    // after PumpEvents is valid and simpler. BUT we need to handle "lost focus"
    // release rules manually then. Let's stick to event-driven for keys/buttons
    // for now to maintain m_liveState fully myself.

  case SDL_CONTROLLERBUTTONDOWN:
  case SDL_CONTROLLERBUTTONUP:
    // We will poll gamepad state in update() to ensure full sync
    break;
  }
}

void InputManager::updateMouseCoordinates() {
  int mx, my;
  SDL_GetMouseState(&mx, &my);

  // Map to console space using normative reference implementation from spec
  // (9.5.4.1) Simplified for now: Fit mode (letterboxed)

  if (mx < m_viewport.x || mx >= m_viewport.x + m_viewport.w ||
      my < m_viewport.y || my >= m_viewport.y + m_viewport.h) {
    m_liveState.mouse.x = -1;
    m_liveState.mouse.y = -1;
    return;
  }

  double localX = mx - m_viewport.x;
  double localY = my - m_viewport.y;

  m_liveState.mouse.x =
      static_cast<int>(localX * m_viewport.canvasW / m_viewport.w);
  m_liveState.mouse.y =
      static_cast<int>(localY * m_viewport.canvasH / m_viewport.h);

  // Gamepad Polling
  for (int i = 0; i < kMaxGamepads; ++i) {
    if (m_controllers[i]) {
      auto &pad = m_liveState.gamepads[i];
      if (!SDL_GameControllerGetAttached(m_controllers[i])) {
        pad.connected = false;
        continue;
      }
      pad.buttons = 0;
      // Map buttons to bitmask
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_A))
        pad.buttons |= (1 << (int)GamepadButton::A);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_B))
        pad.buttons |= (1 << (int)GamepadButton::B);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_X))
        pad.buttons |= (1 << (int)GamepadButton::X);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_Y))
        pad.buttons |= (1 << (int)GamepadButton::Y);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
        pad.buttons |= (1 << (int)GamepadButton::LB);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
        pad.buttons |= (1 << (int)GamepadButton::RB);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_BACK))
        pad.buttons |= (1 << (int)GamepadButton::Back);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_START))
        pad.buttons |= (1 << (int)GamepadButton::Start);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_LEFTSTICK))
        pad.buttons |= (1 << (int)GamepadButton::LS);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_RIGHTSTICK))
        pad.buttons |= (1 << (int)GamepadButton::RS);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_DPAD_UP))
        pad.buttons |= (1 << (int)GamepadButton::DpadUp);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        pad.buttons |= (1 << (int)GamepadButton::DpadDown);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        pad.buttons |= (1 << (int)GamepadButton::DpadLeft);
      if (SDL_GameControllerGetButton(m_controllers[i],
                                      SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        pad.buttons |= (1 << (int)GamepadButton::DpadRight);

      // Axes
      auto getAxis = [&](SDL_GameControllerAxis axis) {
        return SDL_GameControllerGetAxis(m_controllers[i], axis) / 32767.0f;
      };
      pad.axes[(int)GamepadAxis::LeftX] = getAxis(SDL_CONTROLLER_AXIS_LEFTX);
      pad.axes[(int)GamepadAxis::LeftY] = getAxis(SDL_CONTROLLER_AXIS_LEFTY);
      pad.axes[(int)GamepadAxis::RightX] = getAxis(SDL_CONTROLLER_AXIS_RIGHTX);
      pad.axes[(int)GamepadAxis::RightY] = getAxis(SDL_CONTROLLER_AXIS_RIGHTY);
      pad.axes[(int)GamepadAxis::TriggerLeft] =
          getAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
      pad.axes[(int)GamepadAxis::TriggerRight] =
          getAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    }
  }
}

// Queries

bool InputManager::isKeyDown(int scancode) const {
  if (scancode < 0 || scancode >= 512)
    return false;
  return m_currentSnapshot.keys[scancode] != 0;
}

bool InputManager::isKeyPressed(int scancode) const {
  if (scancode < 0 || scancode >= 512)
    return false;
  return m_currentSnapshot.keys[scancode] != 0 &&
         m_previousSnapshot.keys[scancode] == 0;
}

bool InputManager::isKeyReleased(int scancode) const {
  if (scancode < 0 || scancode >= 512)
    return false;
  return m_currentSnapshot.keys[scancode] == 0 &&
         m_previousSnapshot.keys[scancode] != 0;
}

bool InputManager::isMouseButtonDown(int btn) const {
  return (m_currentSnapshot.mouse.buttons & (1 << btn)) != 0;
}

bool InputManager::isMouseButtonPressed(int btn) const {
  bool down = (m_currentSnapshot.mouse.buttons & (1 << btn)) != 0;
  bool prev = (m_previousSnapshot.mouse.buttons & (1 << btn)) != 0;
  return down && !prev;
}

bool InputManager::isMouseButtonReleased(int btn) const {
  bool down = (m_currentSnapshot.mouse.buttons & (1 << btn)) != 0;
  bool prev = (m_previousSnapshot.mouse.buttons & (1 << btn)) != 0;
  return !down && prev;
}

int InputManager::getGamepadCount() const { return kMaxGamepads; }

bool InputManager::isGamepadButtonDown(int padIdx, int btn) const {
  if (padIdx < 0 || padIdx >= kMaxGamepads)
    return false;
  return (m_currentSnapshot.gamepads[padIdx].buttons & (1 << btn)) != 0;
}

bool InputManager::isGamepadButtonPressed(int padIdx, int btn) const {
  if (padIdx < 0 || padIdx >= kMaxGamepads)
    return false;
  bool down = (m_currentSnapshot.gamepads[padIdx].buttons & (1 << btn)) != 0;
  bool prev = (m_previousSnapshot.gamepads[padIdx].buttons & (1 << btn)) != 0;
  return down && !prev;
}

bool InputManager::isGamepadButtonReleased(int padIdx, int btn) const {
  if (padIdx < 0 || padIdx >= kMaxGamepads)
    return false;
  bool down = (m_currentSnapshot.gamepads[padIdx].buttons & (1 << btn)) != 0;
  bool prev = (m_previousSnapshot.gamepads[padIdx].buttons & (1 << btn)) != 0;
  return !down && prev;
}

f32 InputManager::getGamepadAxis(int padIdx, int axis) const {
  if (padIdx < 0 || padIdx >= kMaxGamepads)
    return 0.0f;
  if (axis < 0 || axis >= kGamepadAxisCount)
    return 0.0f;
  return m_currentSnapshot.gamepads[padIdx].axes[axis];
}

} // namespace arcanee::input
