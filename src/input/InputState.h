#pragma once

#include "common/Types.h"
#include <array>
#include <vector>

namespace arcanee::input {

constexpr int kMaxGamepads = 4;
constexpr int kGamepadAxisCount = 6;

// Standard Gamepad Button Mapping (Xbox-style)
enum class GamepadButton : u8 {
  A = 0,
  B = 1,
  X = 2,
  Y = 3,
  LB = 4,
  RB = 5,
  Back = 6,
  Start = 7,
  LS = 8,
  RS = 9,
  DpadUp = 10,
  DpadDown = 11,
  DpadLeft = 12,
  DpadRight = 13,
  Count = 14
};

// Standard Gamepad Axis Mapping
enum class GamepadAxis : u8 {
  LeftX = 0,
  LeftY = 1,
  RightX = 2,
  RightY = 3,
  TriggerLeft = 4,
  TriggerRight = 5,
  Count = 6
};

// Mouse Buttons
constexpr int kMouseButtonLeft = 0;
constexpr int kMouseButtonMiddle = 1;
constexpr int kMouseButtonRight = 2;
constexpr int kMouseButtonX1 = 3;
constexpr int kMouseButtonX2 = 4;

struct MouseState {
  int x = -1;     // Console space X
  int y = -1;     // Console space Y
  u8 buttons = 0; // Bitmask (1 << kMouseButton...)
  f32 wheelX = 0.0f;
  f32 wheelY = 0.0f;
};

struct GamepadState {
  bool connected = false;
  u16 buttons = 0; // Bitmask of GamepadButton
  std::array<f32, kGamepadAxisCount> axes{};
};

/**
 * @brief Represents the input state at a specific tick.
 */
struct InputSnapshot {
  MouseState mouse;
  std::array<GamepadState, kMaxGamepads> gamepads;

  // SDL Scancodes (max 512). 1 if down, 0 if up.
  // Using u8 array for simple access, could be bitset.
  u8 keys[512] = {0};
};

} // namespace arcanee::input
