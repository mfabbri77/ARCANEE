# Appendix B — Enums, Constants, and Mappings (ARCANEE v0.1)

## B.1 Scope

This appendix defines the **standard constants** and **string enumerations** referenced throughout ARCANEE v0.1, including:

* Present/scaling modes
* Aspect and preset identifiers
* Blend modes (2D)
* Text alignment and baseline
* Mouse button constants
* Gamepad standardized mapping (buttons and axes)
* Keyboard key codes (normative policy)
* Standard error codes (optional but recommended)

Unless stated otherwise, all constants are available to cartridges as fields in a global table:

* `const` (recommended) or `sys.const` (acceptable)
  The runtime MUST document the chosen location and keep it stable.

---

## B.2 Display and Presentation Constants

### B.2.1 Aspect Modes (strings)

* `ASPECT_16_9` = `"16:9"`
* `ASPECT_4_3`  = `"4:3"`
* `ASPECT_ANY`  = `"any"`

### B.2.2 Resolution Presets (strings)

* `PRESET_LOW`    = `"low"`
* `PRESET_MEDIUM` = `"medium"`
* `PRESET_HIGH`   = `"high"`
* `PRESET_ULTRA`  = `"ultra"`

### B.2.3 Present/Scaling Modes (strings)

* `SCALE_FIT`             = `"fit"`
* `SCALE_INTEGER_NEAREST` = `"integer_nearest"`
* `SCALE_FILL`            = `"fill"`
* `SCALE_STRETCH`         = `"stretch"`

### B.2.4 Letterbox Clear Color

The default letterbox clear color is:

* `0xFF000000` (opaque black)

If configurable, the default MUST remain black.

---

## B.3 2D Canvas Constants

### B.3.1 Blend Modes (strings)

Supported (v0.1 required):

* `BLEND_NORMAL`      = `"normal"`
* `BLEND_MULTIPLY`    = `"multiply"`
* `BLEND_SCREEN`      = `"screen"`
* `BLEND_OVERLAY`     = `"overlay"`
* `BLEND_DARKEN`      = `"darken"`
* `BLEND_LIGHTEN`     = `"lighten"`
* `BLEND_COLOR_DODGE` = `"colorDodge"`
* `BLEND_COLOR_BURN`  = `"colorBurn"`
* `BLEND_HARD_LIGHT`  = `"hardLight"`
* `BLEND_SOFT_LIGHT`  = `"softLight"`
* `BLEND_DIFFERENCE`  = `"difference"`
* `BLEND_EXCLUSION`   = `"exclusion"`
* `BLEND_ADD`         = `"add"`
* `BLEND_SRC_OVER`    = `"srcOver"` *(may be treated as alias to normal source-over if backend matches)*

Reserved / not supported in v0.1 (must fail safely):

* `BLEND_HUE`         = `"hue"`
* `BLEND_SATURATION`  = `"saturation"`
* `BLEND_COLOR`       = `"color"`
* `BLEND_LUMINOSITY`  = `"luminosity"`
* `BLEND_HARD_MIX`    = `"hardMix"`

### B.3.2 Line Join (strings)

* `LINEJOIN_MITER` = `"miter"`
* `LINEJOIN_ROUND` = `"round"`
* `LINEJOIN_BEVEL` = `"bevel"`

### B.3.3 Line Cap (strings)

* `LINECAP_BUTT`   = `"butt"`
* `LINECAP_ROUND`  = `"round"`
* `LINECAP_SQUARE` = `"square"`

### B.3.4 Gradient Spread Modes (strings)

* `SPREAD_PAD`     = `"pad"`
* `SPREAD_REPEAT`  = `"repeat"`
* `SPREAD_REFLECT` = `"reflect"`

### B.3.5 Text Alignment (strings)

* `TEXTALIGN_LEFT`   = `"left"`
* `TEXTALIGN_CENTER` = `"center"`
* `TEXTALIGN_RIGHT`  = `"right"`
* `TEXTALIGN_START`  = `"start"`
* `TEXTALIGN_END`    = `"end"`

### B.3.6 Text Baseline (strings)

* `TEXTBASELINE_TOP`        = `"top"`
* `TEXTBASELINE_MIDDLE`     = `"middle"`
* `TEXTBASELINE_ALPHABETIC` = `"alphabetic"`
* `TEXTBASELINE_BOTTOM`     = `"bottom"`

---

## B.4 Mouse Constants

### B.4.1 Mouse Buttons (integers)

Normative mapping:

* `MOUSE_LEFT`   = 0
* `MOUSE_MIDDLE` = 1
* `MOUSE_RIGHT`  = 2
* `MOUSE_X1`     = 3
* `MOUSE_X2`     = 4

---

## B.5 Gamepad Constants (Standardized Layout)

### B.5.1 Gamepad Buttons (integers)

Normative mapping (Xbox-style):

* `PAD_A` = 0
* `PAD_B` = 1
* `PAD_X` = 2
* `PAD_Y` = 3
* `PAD_LB` = 4
* `PAD_RB` = 5
* `PAD_BACK`  = 6
* `PAD_START` = 7
* `PAD_LS` = 8
* `PAD_RS` = 9
* `PAD_DPAD_UP`    = 10
* `PAD_DPAD_DOWN`  = 11
* `PAD_DPAD_LEFT`  = 12
* `PAD_DPAD_RIGHT` = 13

### B.5.2 Gamepad Axes (integers)

Normative mapping:

* `PAD_AXIS_LX` = 0
* `PAD_AXIS_LY` = 1
* `PAD_AXIS_RX` = 2
* `PAD_AXIS_RY` = 3
* `PAD_AXIS_LT` = 4
* `PAD_AXIS_RT` = 5

### B.5.3 Axis Ranges

* Sticks: `[-1, +1]` floats
* Triggers: either `[-1,+1]` or `[0,+1]` depending on platform mapping;
  **Normative v0.1:** ARCANEE MUST normalize triggers to `[0, +1]` for consistency.

### B.5.4 Deadzones (Policy)

Recommended defaults (Workbench-configurable):

* Stick radial deadzone: 0.15
* Trigger deadzone: 0.05
  The runtime MUST apply deadzones deterministically.

---

## B.6 Keyboard Key Codes (Normative Policy)

### B.6.1 Requirement

ARCANEE MUST define a stable, cross-platform keyboard code system. v0.1 mandates:

* **Use SDL Scancodes as the canonical key code values** exposed to cartridges.
  Rationale: scancodes represent physical key positions and are more layout-stable for games.

### B.6.2 Exposure

The runtime MUST expose at least the common keys as named constants:
Examples (non-exhaustive; minimum set required):

* `KEY_A`..`KEY_Z`
* `KEY_0`..`KEY_9`
* `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`
* `KEY_SPACE`, `KEY_ENTER`, `KEY_ESCAPE`, `KEY_TAB`, `KEY_BACKSPACE`
* `KEY_LSHIFT`, `KEY_RSHIFT`, `KEY_LCTRL`, `KEY_RCTRL`, `KEY_LALT`, `KEY_RALT`
* `KEY_F1`..`KEY_F12`

**Normative:** The integer values MUST correspond to SDL scancode values for the built SDL version, and the mapping MUST be documented in developer docs.

### B.6.3 Layout-Dependent Text Entry

ARCANEE v0.1 does not expose text input to cartridges by default. If a future text API is added, it must be separate from scancodes and follow Workbench focus rules.

---

## B.7 Standard Error Codes (Recommended)

ARCANEE primarily uses `sys.getLastError()` strings. However, implementations are RECOMMENDED to also provide numeric error codes for programmatic checks:

### B.7.1 Error Code Constants

* `ERR_NONE`              = 0
* `ERR_INVALID_ARGUMENT`  = 1
* `ERR_INVALID_HANDLE`    = 2
* `ERR_PERMISSION_DENIED` = 3
* `ERR_IO_ERROR`          = 4
* `ERR_QUOTA_EXCEEDED`    = 5
* `ERR_UNSUPPORTED`       = 6
* `ERR_BUDGET_OVERRUN`    = 7
* `ERR_HANG_DETECTED`     = 8
* `ERR_DEVICE_ERROR`      = 9
* `ERR_AUDIO_DEVICE`      = 10
* `ERR_ASSET_DECODE`      = 11

### B.7.2 Optional API

If numeric codes are implemented, add:

* `sys.getLastErrorCode() -> int`
* `sys.clearLastErrorCode() -> void`
  and keep them consistent with the string errors.

---

## B.8 Compliance Checklist (Appendix B)

An implementation is compliant with Appendix B if:

1. It exposes all enumerations and constants listed here with the exact values (strings or integers).
2. It uses SDL scancodes as canonical key codes and documents the mapping.
3. It normalizes triggers to `[0,+1]` and applies deadzones deterministically.
4. It rejects unsupported blend modes deterministically and sets last error.
