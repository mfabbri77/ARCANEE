# Comprehensive Verification Plan

## Goal
Verify the correctness and spec-compliance of the implemented Gfx, Audio, and Input systems by creating targeted test cartridges. Ensure `arcanee` can load arbitrary cartridges via command line.

## 1. Runtime Enhancements
### [MODIFY] [src/app/main.cpp](file:///home/mfabbri/Projects/ARCANEE/src/app/main.cpp)
- Parse command line arguments.
- Pass cartridge path to `Runtime`.

### [MODIFY] [src/app/Runtime.h/cpp](file:///home/mfabbri/Projects/ARCANEE/src/app/Runtime.h)
- Add `loadCartridge(std::string path)` method (or constructor arg).
- Remove hardcoded "samples/hello".

## 2. Test Cartridges
Create new folders in `samples/` with specific `main.nut` scripts.

### 2.1 Graphics Test (`samples/test_gfx`)
- **Features to test**:
  - `gfx.cls(color)`
  - `gfx.color(c)`
  - `gfx.line(x1,y1,x2,y2)`
  - `gfx.rect(x,y,w,h,fill)`
  - `gfx.circ(x,y,r,fill)`
  - `gfx.print(x,y,str)` (if available, otherwise rely on logging)
- **Validation**: Visual inspection of shapes and colors.

### 2.2 Input Test (`samples/test_input`)
- **Features to test**:
  - `inp.btn(id)`, `inp.btnp(id)` (Keyboard/Gamepad)
  - `inp.mouse_x()`, `inp.mouse_y()`, `inp.mouse_btn()`
- **Validation**:
  - Move a shape with D-pad/Arrow keys.
  - Change color on button press.
  - Draw cursor at mouse position.

### 2.3 Audio Test (`samples/test_audio`)
- **Features to test**:
  - `audio.loadSound()`, `audio.playSound()`
  - `audio.loadModule()`, `audio.playModule()`
- **Validation**:
  - Trigger SFX on key press.
  - Start/Stop music.

## 3. Spec Review
- Compare `gfx.*`, `inp.*`, `audio.*` Implementation vs Specs (Chapters 6, 8, 9).
- Document any deviations.

## 4. Execution
- Run: `./bin/arcanee samples/test_gfx`
- Run: `./bin/arcanee samples/test_input`
- Run: `./bin/arcanee samples/test_audio`
