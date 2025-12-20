# Chapter 6 — 2D Canvas API (ThorVG-backed) (ARCANEE v0.1)

## 6.1 Overview

ARCANEE v0.1 exposes a **Canvas-like 2D API** to cartridges under the namespace `gfx.*`. The API is inspired by the HTML5 Canvas 2D context model (state stack, transforms, paths, fill/stroke), but is adapted to ARCANEE’s sandbox and handle-based resource model.

The 2D backend is **ThorVG**. In v0.1, the reference implementation is **CPU rasterization** (ThorVG software canvas) into a 32bpp surface, followed by GPU upload and compositing as specified in Chapter 5.

This chapter defines:

* Coordinate system and drawing model
* State stack and transforms
* Path construction and semantics
* Fill, stroke, and paint sources (solid color + gradients in v0.1)
* Blend modes (subset aligned with ThorVG supported modes)
* Clipping model
* Images and surfaces
* Text and fonts (basic model)
* Limits, determinism, error handling, and performance requirements

---

## 6.2 Coordinate System (Normative)

### 6.2.1 Space

All `gfx.*` drawing occurs in **Console Space** unless an offscreen surface is bound:

* Origin: top-left
* Units: pixels
* Axis: +X to the right, +Y downward

### 6.2.2 Precision

API arguments are floats; implementations MUST preserve sufficient precision for typical use. However:

* In `present.mode = integer_nearest`, pixel-perfect output is best achieved when drawing on integer coordinates (informative guidance).

### 6.2.3 Clipping to Target Bounds

All drawing is implicitly clipped to the current render target bounds (`CBUF` or current surface). Drawing outside bounds MUST NOT crash and MAY be discarded.

---

## 6.3 Public API Surface (Normative)

### 6.3.1 Targets and Clearing

* `gfx.createSurface(w: int, h: int) -> SurfaceHandle|0`
* `gfx.freeSurface(s: SurfaceHandle) -> void`
* `gfx.setSurface(s: SurfaceHandle|null) -> void`

  * `null` binds the Console Framebuffer (CBUF)
* `gfx.clear(color: u32 = 0x00000000) -> void`
* `gfx.getTargetSize() -> (int w, int h)`

  * Returns current target dimensions (CBUF or surface)

### 6.3.2 State Stack and Global State

* `gfx.save() -> void`
* `gfx.restore() -> void`
* `gfx.resetTransform() -> void`
* `gfx.setTransform(a: float,b: float,c: float,d: float,e: float,f: float) -> void`
* `gfx.translate(x: float, y: float) -> void`
* `gfx.rotate(rad: float) -> void`
* `gfx.scale(x: float, y: float) -> void`
* `gfx.setGlobalAlpha(a: float) -> void`  // clamps to [0,1]
* `gfx.setBlend(mode: string) -> bool`

  * Returns `false` if mode unsupported (sets last error)

### 6.3.3 Styles

* `gfx.setFillColor(color: u32) -> void`
* `gfx.setStrokeColor(color: u32) -> void`
* `gfx.setLineWidth(px: float) -> void`
* `gfx.setLineJoin(join: string) -> bool`   // miter|round|bevel
* `gfx.setLineCap(cap: string) -> bool`     // butt|round|square
* `gfx.setMiterLimit(v: float) -> void`
* `gfx.setLineDash(dashes: array<float>|null, offset: float=0) -> bool`

### 6.3.4 Paths

* `gfx.beginPath() -> void`
* `gfx.closePath() -> void`
* `gfx.moveTo(x: float, y: float) -> void`
* `gfx.lineTo(x: float, y: float) -> void`
* `gfx.quadTo(cx: float, cy: float, x: float, y: float) -> void`
* `gfx.cubicTo(c1x: float, c1y: float, c2x: float, c2y: float, x: float, y: float) -> void`
* `gfx.arc(x: float, y: float, r: float, a0: float, a1: float, ccw: bool=false) -> void`
* `gfx.rect(x: float, y: float, w: float, h: float) -> void`

### 6.3.5 Drawing Operations

* `gfx.fill() -> void`
* `gfx.stroke() -> void`
* `gfx.clip() -> void`
* `gfx.resetClip() -> void`
* `gfx.fillRect(x: float, y: float, w: float, h: float) -> void`
* `gfx.strokeRect(x: float, y: float, w: float, h: float) -> void`
* `gfx.clearRect(x: float, y: float, w: float, h: float) -> void`

### 6.3.6 Images

* `gfx.loadImage(path: string) -> ImageHandle|0`
* `gfx.freeImage(img: ImageHandle) -> void`
* `gfx.getImageSize(img: ImageHandle) -> (int w, int h)`
* `gfx.drawImage(img: ImageHandle, dx: float, dy: float) -> void`
* `gfx.drawImageRect(img: ImageHandle,
                     sx: int, sy: int, sw: int, sh: int,
                     dx: float, dy: float, dw: float, dh: float) -> void`

### 6.3.7 Paint Objects (Gradients) — v0.1 Optional but Supported

ARCANEE v0.1 SHOULD support linear gradients (minimum). If gradients are not implemented, these APIs MUST exist but fail safely.

* `gfx.createLinearGradient(x1: float, y1: float, x2: float, y2: float) -> PaintHandle|0`
* `gfx.createRadialGradient(cx: float, cy: float, r: float) -> PaintHandle|0`
* `gfx.paintSetStops(p: PaintHandle, stops: array<table>) -> bool`

  * stop format: `{ offset: float (0..1), color: u32 }`
* `gfx.paintSetSpread(p: PaintHandle, mode: string) -> bool` // pad|repeat|reflect
* `gfx.paintFree(p: PaintHandle) -> void`
* `gfx.setFillPaint(p: PaintHandle|null) -> void`
* `gfx.setStrokePaint(p: PaintHandle|null) -> void`

### 6.3.8 Text

* `gfx.loadFont(path: string, sizePx: int, opts: table=null) -> FontHandle|0`
* `gfx.freeFont(font: FontHandle) -> void`
* `gfx.setFont(font: FontHandle) -> void`
* `gfx.setTextAlign(align: string) -> bool` // left|center|right|start|end
* `gfx.setTextBaseline(base: string) -> bool` // top|middle|alphabetic|bottom
* `gfx.measureText(text: string) -> table|null`

  * `{ width, height, ascent, descent, lineHeight }`
* `gfx.fillText(text: string, x: float, y: float, maxWidth: float=null) -> void`
* `gfx.strokeText(text: string, x: float, y: float, maxWidth: float=null) -> void`

---

## 6.4 Canvas State Model (Normative)

### 6.4.1 State Stack

`gfx.save()` MUST push the following state:

* Current transform (2D affine matrix)
* Global alpha
* Blend mode
* Fill source: (solid color or paint handle)
* Stroke source: (solid color or paint handle)
* Stroke parameters (line width, join/cap, miter limit, dash array + offset)
* Font handle + text align + baseline
* Clip state

`gfx.restore()` MUST restore the most recently saved state.

* If restore is called with an empty stack, it MUST fail safely (no crash) and set last error.

### 6.4.2 Default State

On binding a target (CBUF or surface) or at frame start (policy-defined), the default state MUST be:

* Identity transform
* Global alpha = 1
* Blend = `"normal"`
* Fill color = opaque white (`0xFFFFFFFF`)
* Stroke color = opaque black (`0xFF000000`)
* Line width = 1
* Line join = miter, cap = butt, miter limit = 10
* Dash disabled
* No clip
* No font (text drawing fails safely unless a font is set)

---

## 6.5 Transforms (Normative)

### 6.5.1 Matrix Definition

`setTransform(a,b,c,d,e,f)` defines the 2D affine transform:

```
| a c e |
| b d f |
| 0 0 1 |
```

### 6.5.2 Operation Order

`translate/rotate/scale/transform` MUST post-multiply the current transform (Canvas-like behavior). Implementations MUST document if they differ; v0.1 requires Canvas-compatible semantics.

### 6.5.3 Reset

`resetTransform()` restores identity.

---

## 6.6 Path Construction Semantics (Normative)

### 6.6.1 Current Path

`beginPath()` clears the current path.
Path commands append segments to the current path.

### 6.6.2 Subpaths and Close

* `moveTo` starts a new subpath.
* `closePath` closes the current subpath with a segment from the current point to the subpath start point.

### 6.6.3 Arc Semantics

`arc(x,y,r,a0,a1,ccw)`:

* Angles are in radians.
* `ccw=false` draws the shorter clockwise path from `a0` to `a1` as in common canvas conventions.
* If `r <= 0`, the call fails safely.

Implementations SHOULD match HTML5 canvas arc direction semantics. If deviation exists, it MUST be documented.

### 6.6.4 Rect Convenience

`rect(x,y,w,h)` appends a rectangle path.
If `w<0` or `h<0`, the call fails safely.

---

## 6.7 Fill and Stroke Semantics (Normative)

### 6.7.1 Fill Rule

ARCANEE v0.1 uses the **non-zero winding rule** for fills unless explicitly extended in v0.2.
A future `gfx.setFillRule("nonzero|evenodd")` may be added; v0.1 is fixed to non-zero.

### 6.7.2 Fill Source Resolution

When `gfx.fill()` is called:

* If `setFillPaint(p)` is active and valid, it is used.
* Otherwise the solid fill color is used.

Similarly for `stroke()` and stroke sources.

### 6.7.3 Global Alpha

Global alpha multiplies the alpha component of the fill/stroke source.

### 6.7.4 Stroke Parameters

* Line width is in pixels in current user space.
* Dash arrays are in pixels and are affected by the current transform (implementation detail). v0.1 RECOMMENDS: dashes follow user space similarly to canvas.

---

## 6.8 Blend Modes (Normative)

### 6.8.1 Supported Modes

`gfx.setBlend(mode)` MUST support at minimum:

* `"normal"`
* `"multiply"`
* `"screen"`
* `"overlay"`
* `"darken"`
* `"lighten"`
* `"colorDodge"`
* `"colorBurn"`
* `"hardLight"`
* `"softLight"`
* `"difference"`
* `"exclusion"`
* `"add"`
* `"srcOver"` (if exposed; otherwise alias to normal source-over)

If a requested mode is not supported by the backend, the call MUST return `false` and set last error.

### 6.8.2 Not Supported in v0.1

These modes MUST be rejected (return false) in v0.1:

* `"hue"`, `"saturation"`, `"color"`, `"luminosity"`, `"hardMix"`

---

## 6.9 Clipping (Normative)

### 6.9.1 Clip Operation

`gfx.clip()` intersects the current clip region with the **current path**, transformed by the current transform.

### 6.9.2 Clip Stack

Clip state MUST be part of the `save()/restore()` stack.

* `save()` captures clip.
* `restore()` restores prior clip.

### 6.9.3 resetClip

`gfx.resetClip()` resets the clip region to “no clip” for the current state level (does not affect saved states below).

---

## 6.10 Images (Normative)

### 6.10.1 Loading

`gfx.loadImage(path)`:

* Path MUST be within `cart:/`, `save:/`, or `temp:/` subject to permissions.
* Supported formats are implementation-defined in v0.1; the runtime MUST document the supported formats. PNG is RECOMMENDED.

### 6.10.2 Sampling and Filtering

Image sampling during `drawImage` is controlled by runtime policy:

* In `present.mode=integer_nearest`, the *final presentation* is nearest-filtered, but images drawn into CBUF may still be filtered depending on the canvas backend.
* v0.1 RECOMMENDS providing:

  * `gfx.setImageFilter("nearest|linear")` as a runtime option in Workbench (may be v0.2 if not implemented).

### 6.10.3 drawImageRect Bounds

If source rect is out of bounds, the runtime MUST clamp or fail safely (implementation choice), but MUST NOT crash.

---

## 6.11 Surfaces (Offscreen Rendering) (Normative)

### 6.11.1 Surface Properties

* Surfaces are 32bpp.
* Surfaces do not implicitly resize.
* Surface memory contributes to quota/limits (Chapter 12).

### 6.11.2 Surface Rendering

* `gfx.setSurface(surface)` redirects subsequent canvas operations to that surface.
* Drawing a surface into CBUF is v0.1 implementation-defined; RECOMMENDED:

  * expose surfaces as ImageHandles via `gfx.surfaceToImage(surface)` (v0.2)
  * or provide `gfx.drawSurface(surface, ...)` in v0.1 if needed.

---

## 6.12 Text and Fonts (Normative)

### 6.12.1 Text Encoding

Text inputs MUST be treated as UTF-8.

### 6.12.2 Font Loading

`gfx.loadFont(path, sizePx, opts)`:

* Loads a font file (TTF/OTF support is implementation-defined; TTF is RECOMMENDED).
* `sizePx` is the nominal pixel size used for metrics and rasterization.
* If load fails, return `0` and set last error.

### 6.12.3 Font State

Text drawing requires a valid font set via `gfx.setFont(font)`:

* If no font is set, `fillText/strokeText/measureText` MUST fail safely.

### 6.12.4 Align and Baseline

`setTextAlign` and `setTextBaseline` modify how `(x,y)` is interpreted:

Align:

* `left|start`: x is left edge
* `center`: x is horizontal center
* `right|end`: x is right edge

Baseline:

* `top`: y is top edge of text box
* `middle`: y is vertical center
* `alphabetic`: y is alphabetic baseline
* `bottom`: y is bottom edge

The runtime MUST implement these offsets consistently for both `measureText` and rendering.

### 6.12.5 measureText Output

`measureText(text)` MUST return:

* `width`: advance width
* `height`: total height (ascent + descent, minimum line height)
* `ascent`, `descent`, `lineHeight`

Values MUST be consistent across platforms within reasonable tolerance. If platform font rasterization differs, the runtime SHOULD pin to a single font engine behavior.

### 6.12.6 maxWidth

If `maxWidth` is specified:

* v0.1 MAY implement horizontal scaling or clipping. If not implemented, it MUST be ignored consistently and documented.
* v0.1 RECOMMENDS clipping to `maxWidth` for predictability.

---

## 6.13 Limits and Performance Requirements (Normative)

### 6.13.1 Canvas Complexity Limits

The runtime MUST enforce limits to prevent pathological workloads:

* Maximum canvas target pixel count (from caps/policy)
* Maximum path segment count per frame (policy-defined)
* Maximum `save()` stack depth (policy-defined; RECOMMENDED default 64)

If exceeded:

* The runtime MUST fail safely, set last error, and in Workbench provide diagnostics.

### 6.13.2 Determinism Constraints

Given identical:

* cartridge code,
* inputs per tick,
* and configuration (CBUF size, present mode, etc.),
  the canvas output into CBUF MUST be functionally deterministic.

GPU driver differences may affect subtle antialiasing results; v0.1 does not require bit-identical pixel output across GPUs but requires stable, non-random behavior.

---

## 6.14 Error Handling (Normative)

Every `gfx.*` function MUST:

* Validate arguments and handle validity.
* On failure, set `sys.getLastError()` and return:

  * `false` for boolean-returning calls
  * `0` for handle-returning calls
  * `null` for table-returning calls
  * For void calls: no-op + set last error (Dev Mode SHOULD log)

---

## 6.15 Compliance Checklist (Chapter 6)

An implementation is compliant with Chapter 6 if it:

1. Implements the full `gfx.*` API surface listed in §6.3 (functions may fail safely for optional features, but must exist).
2. Implements a Canvas-like state stack and transform semantics.
3. Implements path building with fill/stroke, solid colors, global alpha, and clipping.
4. Supports the required blend modes and rejects unsupported modes deterministically.
5. Supports image loading and drawing with sandboxed VFS paths.
6. Implements text loading/measurement/rendering with align and baseline rules.
7. Enforces canvas limits and fails safely with proper error reporting.

---

Next: **Chapter 7 — 3D Scene API and Rendering Model (Diligent-based)** (scene graph, entities, transforms, glTF loading, materials, lights, camera, render stages, and v0.1 post effects policy).
