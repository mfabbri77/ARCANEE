# Chapter 9 — Input System and API (SDL2-backed, Deterministic Snapshots) (ARCANEE v0.1)

## 9.1 Overview

ARCANEE v0.1 provides a deterministic, frame-stable input system exposed to cartridges via `inp.*`. Input is collected from the platform layer (SDL2) and converted into **per-tick snapshots** so that:

* all input queries inside a single `update(dt_fixed)` observe a consistent state
* input timing is deterministic relative to the engine tick loop (Chapter 4)
* Workbench UI can capture input without leaking accidental keystrokes to the cartridge

This chapter specifies:

* Input capture and snapshot timing
* Keyboard, mouse, and gamepad semantics
* Focus and capture rules in Workbench
* Coordinate spaces and scaling considerations
* API definitions and failure behavior
* Determinism and replay considerations (forward compatibility)

---

## 9.2 Input Timing and Snapshot Model (Normative)

### 9.2.1 Input Sampling Points

ARCANEE MUST sample input from SDL events during the **Event Pump** phase of the main loop (Chapter 2). Input state is then stored into a **Current Input State** structure.

For fixed-step simulation:

* At the start of each `update(dt_fixed)` call, ARCANEE MUST freeze the current input state into a **Tick Input Snapshot**.
* All `inp.*` queries during that update MUST read from the Tick Input Snapshot (not from live event state).

### 9.2.2 Edge Events: pressed/released

ARCANEE MUST track edges (transitions) relative to tick snapshots:

* `pressed` is true for the first tick in which the key/button becomes down.
* `released` is true for the first tick in which it becomes up.

Edges MUST be computed against the previous tick snapshot for determinism.

### 9.2.3 Behavior When Multiple Updates per Frame Occur

When a frame executes multiple `update()` ticks (catch-up):

* Each tick MUST use the same “frozen” input snapshot unless new SDL events have been processed between ticks.
* v0.1 requirement: SDL event pump occurs once per host frame; therefore all updates in that frame use the same input snapshot (deterministic and simple).

This rule MUST be documented for cartridge authors (informative):

* very high input rates (e.g., rapid mouse motion) are effectively sampled at the host frame rate.

---

## 9.3 Focus, Capture, and Workbench Interaction (Normative)

### 9.3.1 Focus Model

ARCANEE distinguishes:

* **Workbench focus** (editing code, typing in UI widgets)
* **Cartridge focus** (game input)

The runtime MUST provide a deterministic rule to decide whether input events are routed to:

* Workbench UI only
* cartridge only
* both (rare; should be avoided)

v0.1 rule (normative):

* If ImGui indicates it wants keyboard or mouse capture for the current frame, the cartridge MUST NOT receive those inputs for that device category.

  * Keyboard: when Workbench is typing in the editor or any text input.
  * Mouse: when interacting with Workbench windows or controls.

### 9.3.2 Run Mode vs Edit Mode

Workbench must support at least:

* **Edit Mode**: cartridge may be stopped; Workbench receives input.
* **Run Mode**: cartridge running; Workbench still visible; input routing depends on capture.
* **Focus Toggle**: a key (e.g., F1 or Esc policy-defined) to toggle “cartridge focus lock”:

  * When enabled, the cartridge receives keyboard/mouse even if Workbench is visible, except for a reserved key to escape focus.
  * This is a Workbench UX feature; behavior MUST be deterministic.

### 9.3.3 Window Inactive Behavior

If the application window loses focus:

* Keyboard and mouse input states MUST be reset to “not pressed” for safety.
* `pressed/released` edges MUST behave deterministically (typically: all keys become released on focus loss).

---

## 9.4 Keyboard Input (Normative)

### 9.4.1 Key Codes

ARCANEE MUST define a stable set of key codes exposed to cartridges:

* v0.1: use SDL scancodes or an ARCANEE-defined enum mapped from SDL scancodes.
* The mapping MUST be documented and stable across platforms.

### 9.4.2 API

* `inp.keyDown(code: int) -> bool`
* `inp.keyPressed(code: int) -> bool`
* `inp.keyReleased(code: int) -> bool`

### 9.4.3 Text Input vs Key Input

For gameplay input, key states are sufficient.
If text input is needed in cartridges (optional in v0.1), it MUST be a separate API with explicit focus rules (recommended for v0.2). In v0.1, cartridges SHOULD not receive raw text input to avoid conflicting with Workbench.

---

## 9.5 Mouse Input (Normative)

### 9.5.1 Coordinate Space

Mouse coordinates returned to cartridges MUST be in **Console Space** by default, because cartridges render in CBUF space.

However, the OS mouse is in Display Space. Therefore ARCANEE MUST define conversion:

* `inp.mousePos()` returns `(x,y)` in **console coordinates** corresponding to where the mouse points within the presented CBUF viewport.
* If the mouse is outside the CBUF viewport (letterboxed area), the runtime MUST return:

  * either clamped coordinates to edge, or
  * a sentinel value (recommended: `(-1,-1)`), deterministically.

v0.1 requirement:

* `inp.mousePos()` returns `(-1,-1)` when outside the viewport.

### 9.5.2 Mouse Buttons and Wheel

* `inp.mouseDown(btn: int) -> bool`
* `inp.mousePressed(btn: int) -> bool`
* `inp.mouseReleased(btn: int) -> bool`
* `inp.mouseWheel() -> (float dx, float dy)`

  * Wheel deltas accumulate within the current tick and reset each tick.

### 9.5.3 Button Codes

ARCANEE MUST define stable button codes:

* 0 = left, 1 = middle, 2 = right, 3 = X1, 4 = X2 (recommended)
  Mapping MUST be documented.

### 9.5.4 Present Mode Interaction

Mouse mapping MUST respect the active present viewport (`fit`, `integer_nearest`, `fill`, `stretch`):

* In `fit` and `integer_nearest`: map to the centered viewport; outside yields (-1,-1).
* In `fill`: because of cropping, mapping MUST invert the crop transform so that visible region corresponds to correct console coordinates.
* In `stretch`: simple scaling by `(x * w / W, y * h / H)`.

These mappings MUST use the same viewport math as Chapter 5 and must be recomputed on window resize.

#### 9.5.4.1 Reference Implementation (Normative)

```c++
// Mouse to console space conversion - NORMATIVE reference implementation

struct ConsolePos { int x, y; bool valid; };

ConsolePos displayToConsole(int mx, int my,           // Mouse display coords
                            int W, int H,             // Backbuffer
                            int w, int h,             // CBUF
                            const Viewport& vp,       // From calculateViewport()
                            PresentMode mode) {
    ConsolePos out = {-1, -1, false};
    
    if (mode == PresentMode::Fill) {
        // Fill mode: viewport may extend beyond backbuffer
        // Map mouse to the portion of CBUF that is visible
        double localX = mx - vp.x;
        double localY = my - vp.y;
        
        out.x = static_cast<int>(localX * w / vp.w);
        out.y = static_cast<int>(localY * h / vp.h);
        
        // Check if within valid CBUF range
        if (out.x >= 0 && out.x < w && out.y >= 0 && out.y < h) {
            out.valid = true;
        }
    }
    else if (mode == PresentMode::Stretch) {
        // Simple proportional mapping
        out.x = mx * w / W;
        out.y = my * h / H;
        out.valid = (out.x >= 0 && out.x < w && out.y >= 0 && out.y < h);
    }
    else {
        // Fit and IntegerNearest: centered viewport with letterbox
        if (mx < vp.x || mx >= vp.x + vp.w ||
            my < vp.y || my >= vp.y + vp.h) {
            // Outside viewport -> invalid
            return out;
        }
        
        // Map within viewport to console space
        double localX = mx - vp.x;
        double localY = my - vp.y;
        
        out.x = static_cast<int>(localX * w / vp.w);
        out.y = static_cast<int>(localY * h / vp.h);
        out.valid = true;
    }
    
    // Final bounds check
    if (!out.valid || out.x < 0 || out.x >= w || out.y < 0 || out.y >= h) {
        return {-1, -1, false};
    }
    
    return out;
}
```

---

## 9.6 Gamepad Input (Normative)

### 9.6.1 Device Model

ARCANEE exposes gamepads as indexed devices:

* `pad` index from 0 to `inp.padCount()-1`
* Device ordering MUST be stable for a session (and may change across restarts)

### 9.6.2 API

* `inp.padCount() -> int`
* `inp.padButtonDown(pad: int, btn: int) -> bool`
* `inp.padButtonPressed(pad: int, btn: int) -> bool`
* `inp.padButtonReleased(pad: int, btn: int) -> bool`
* `inp.padAxis(pad: int, axis: int) -> float`  // [-1, +1]

### 9.6.3 Standardized Layout

ARCANEE v0.1 MUST define a standardized button/axis layout (Xbox-style recommended):
Buttons (example mapping; must be fixed):

* 0=A, 1=B, 2=X, 3=Y
* 4=LB, 5=RB
* 6=Back, 7=Start
* 8=LS, 9=RS
* 10=DpadUp, 11=DpadDown, 12=DpadLeft, 13=DpadRight

Axes:

* 0=LeftX, 1=LeftY, 2=RightX, 3=RightY, 4=LT, 5=RT

SDL’s gamecontroller API is recommended for stable mapping, but the spec requires only that ARCANEE’s mapping is stable and documented.

### 9.6.4 Deadzones

ARCANEE SHOULD apply deadzones deterministically:

* Left/Right sticks: radial deadzone recommended
* Triggers: threshold deadzone recommended

Deadzone values MUST be configurable by Workbench/user settings and applied consistently.

---

## 9.7 Input Limits and Performance

* Input query calls (`inp.*`) MUST be O(1) and allocation-free.
* The runtime MUST bound event queue processing per frame to prevent unbounded work; if event backlog occurs, it MUST be processed deterministically.

---

## 9.8 Error Handling (Normative)

* Invalid `pad` index MUST return safe defaults:

  * buttons: false
  * axes: 0
  * and set last error only in Dev Mode (to avoid spamming in Player mode)
* Invalid key/button codes MUST behave similarly (false/0).

---

## 9.9 Compliance Checklist (Chapter 9)

An implementation is compliant with Chapter 9 if it:

1. Samples SDL input events once per host frame and freezes a tick snapshot for each update tick.
2. Provides deterministic `Down/Pressed/Released` semantics for keyboard, mouse, and gamepad.
3. Implements Workbench capture rules that prevent input leakage when editing text/UI.
4. Converts mouse coordinates to console space using the current present viewport math, returning (-1,-1) when outside the viewport.
5. Implements a stable standardized gamepad mapping and deterministic deadzones.
6. Ensures input APIs are O(1), allocation-free, and safe on invalid indices.
---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
