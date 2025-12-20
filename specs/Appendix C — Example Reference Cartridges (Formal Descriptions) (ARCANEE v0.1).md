# Appendix C — Example Reference Cartridges (Formal Descriptions) (ARCANEE v0.1)

## C.1 Scope

This appendix defines a set of **reference cartridges** intended for:

* onboarding developers implementing ARCANEE
* validating conformance to the API and runtime semantics
* serving as canonical examples for cartridge authors

Each reference cartridge is specified by:

* package layout
* manifest configuration
* required assets
* expected runtime behavior
* validation checkpoints (what a developer should observe in Workbench)

These cartridges are not “sample code” in this appendix; they are **formal testable descriptions**. Implementations MAY ship them as official examples.

---

## C.2 Reference Cartridge: RC-001 “PixelPerfect Runner” (Retro Test)

### C.2.1 Purpose

Validate:

* CBUF presets and aspect selection
* `present.mode = integer_nearest` viewport math
* nearest filtering behavior (no blur)
* deterministic input snapshots and movement
* basic image drawing and minimal text

### C.2.2 Manifest (Normative)

`cartridge.toml`:

```toml
id = "arcanee.ref.rc001.pixelperfect"
title = "RC-001 PixelPerfect Runner"
version = "0.1.0"
api_version = "0.1"
entry = "main.nut"

[display]
aspect = "16:9"
preset = "low"
scaling = "integer_nearest"
allow_user_override = true

[permissions]
save_storage = true
audio = false
net = false
native = false

[caps]
cpu_ms_per_update = 1.0
max_draw_calls = 2000
```

### C.2.3 Package Layout

```
/cartridge.toml
/main.nut
/assets/img/sprite_sheet.png
/assets/fonts/mono.ttf
```

### C.2.4 Runtime Behavior Requirements

* On `init()`:

  * load `sprite_sheet.png`
  * load `mono.ttf` at 12 px
  * initialize player position at center-left
* On `update(dt)`:

  * Arrow keys move the player in 4 directions at constant speed.
  * Movement must be deterministic and tick-based.
* On `draw(alpha)`:

  * clear screen to solid color
  * draw a pixel-art character sprite at integer coordinates (no subpixel)
  * draw a small overlay text:

    * `CBUF: w×h`
    * `DISPLAY: W×H`
    * `MODE: integer_nearest`
    * `K: <integer>` (derived from viewport; displayed value computed by cartridge from `sys.getDisplaySize()` and `sys.getConsoleSize()` using Chapter 5 math)

### C.2.5 Validation Checkpoints

1. In `integer_nearest`, sprite edges must be **pixel-sharp** (no linear blur).
2. Resizing window changes `k` deterministically; `k` must match `floor(min(W/w, H/h))`.
3. When window is too small for `k >= 1`, present mode must fall back to `fit` (runtime), and cartridge may display a warning.
4. Input pressed/released semantics must be stable: holding a key yields continuous movement; `keyPressed` is true only once at press.

---

## C.3 Reference Cartridge: RC-002 “VectorDesk” (Modern 2D Canvas Test)

### C.3.1 Purpose

Validate:

* Canvas state stack, transforms, path fill/stroke
* gradients (if implemented) and blend modes
* clipping behavior and save/restore clip stack
* text metrics and baseline/alignment
* offscreen surfaces (if implemented in v0.1)

### C.3.2 Manifest

```toml
id = "arcanee.ref.rc002.vectordesk"
title = "RC-002 VectorDesk"
version = "0.1.0"
api_version = "0.1"
entry = "main.nut"

[display]
aspect = "16:9"
preset = "high"
scaling = "fit"
allow_user_override = true

[permissions]
save_storage = false
audio = false
net = false
native = false

[caps]
cpu_ms_per_update = 2.0
max_canvas_pixels = 16777216
```

### C.3.3 Layout

```
/cartridge.toml
/main.nut
/assets/fonts/Inter.ttf
```

### C.3.4 Runtime Behavior Requirements

* Draw an animated “desktop” scene composed of:

  * rounded rectangles (implemented via paths)
  * gradients (if supported; otherwise solid fallback is acceptable but must be deterministic)
  * a clipped scrolling panel: content moves behind a clip region
  * blend mode showcase: a grid of squares demonstrating each supported blend mode
* Text:

  * display a baseline/alignment grid:

    * same string drawn with `top/middle/alphabetic/bottom` baselines
    * left/center/right alignment markers
  * verify `measureText` matches actual drawing offsets (within tolerance)

### C.3.5 Validation Checkpoints

1. `save/restore` must correctly restore transforms, clip, colors, and global alpha.
2. `clip()` must intersect with existing clip and be restored by `restore()`.
3. Blend modes:

   * supported modes must visibly differ (especially multiply/screen/overlay/add)
   * unsupported modes must fail safely and not crash; cartridge should show an “unsupported” label.
4. Text alignment and baseline must match specification: changing baseline moves rendered text relative to anchor point, consistent with `measureText`.

---

## C.4 Reference Cartridge: RC-003 “PBR Room + Module Music” (3D + Audio Test)

### C.4.1 Purpose

Validate:

* glTF loading and deterministic entity creation
* PBR rendering pipeline (baseColor/metallicRoughness/normal/emissive)
* camera control and lights
* environment HDR and tonemapping (if implemented)
* module playback via libopenmpt
* integration order: 3D → 2D overlay → present

### C.4.2 Manifest

```toml
id = "arcanee.ref.rc003.pbrroom"
title = "RC-003 PBR Room + Module"
version = "0.1.0"
api_version = "0.1"
entry = "main.nut"

[display]
aspect = "16:9"
preset = "high"
scaling = "fit"
allow_user_override = true

[permissions]
save_storage = true
audio = true
net = false
native = false

[caps]
cpu_ms_per_update = 2.0
audio_channels = 32
```

### C.4.3 Layout

```
/cartridge.toml
/main.nut
/assets/3d/room.glb
/assets/3d/env.hdr              (optional if supported)
/assets/music/track.xm
/assets/fonts/Inter.ttf
```

### C.4.4 Runtime Behavior Requirements

* On `init()`:

  * `gfx3d.loadGLTF("cart:/assets/3d/room.glb")`
  * create a camera; set perspective and initial orbit view
  * create at least one directional light and one point light
  * if HDR environment supported: load env map and set tonemapper to ACES
  * `audio.loadModule("cart:/assets/music/track.xm")` and start playback looped
* On `update(dt)`:

  * WASD: move camera (or orbit) deterministically
  * mouse drag (optional): rotate camera; must use console-mapped coordinates
  * key toggles:

    * toggle tonemapper (aces/reinhard/none)
    * toggle bloom/SSAO if supported; otherwise show unsupported message
    * pause/resume module
* On `draw(alpha)`:

  * call `gfx3d.render()`
  * draw a 2D overlay (Canvas) showing FPS, camera position, module title, and current post settings

### C.4.5 Validation Checkpoints

1. The scene loads deterministically: consistent entity/material counts across runs.
2. 3D renders behind 2D overlay, with stable camera controls.
3. Module playback starts and loops; pause/resume works without glitches.
4. If environment/tonemapping are implemented, toggles change output predictably; if not, calls fail safely and set last error.

---

## C.5 Reference Cartridge: RC-004 “VFS Sandbox & Save” (Storage Test)

### C.5.1 Purpose

Validate:

* VFS namespaces and path normalization
* quota enforcement behavior
* permissions `save_storage` on/off
* stable directory listing order

### C.5.2 Manifest

Two variants SHOULD exist:

**Variant A (Save enabled)**:

```toml
id = "arcanee.ref.rc004.vfs.save"
title = "RC-004 VFS Sandbox Save"
version = "0.1.0"
api_version = "0.1"
entry = "main.nut"

[display]
aspect = "4:3"
preset = "medium"
scaling = "fit"
allow_user_override = true

[permissions]
save_storage = true
audio = false
net = false
native = false
```

**Variant B (Save disabled)**:
Same but `save_storage=false`.

### C.5.3 Behavior Requirements

* On init:

  * list `cart:/` contents and display on screen (sorted)
  * attempt to write to `save:/test.txt`

    * Variant A: must succeed
    * Variant B: must fail with PermissionDenied
  * attempt `fs.readText("cart:/../x")` must fail due to `..` rejection
* On draw:

  * show last error string if any
  * show `fs.stat` results for `save:/test.txt` if present

### C.5.4 Validation Checkpoints

1. `..` traversal is rejected.
2. Directory listing order is stable and lexicographical.
3. Save permission is strictly enforced.
4. Errors are user-readable and do not expose host absolute paths.

---

## C.6 Packaging and Distribution Notes (Informative)

These reference cartridges SHOULD be distributed:

* as directory projects in Workbench
* optionally as `.zip` cartridges to validate archive mounting behavior

---

## C.7 Compliance Checklist (Appendix C)

An implementation meets Appendix C intent if:

1. It can run each reference cartridge as specified without crashes.
2. Observed behaviors match the validation checkpoints.
3. Unsupported optional features fail safely with clear errors.
4. The Workbench can navigate compile/runtime errors to the correct file:line for these projects.
