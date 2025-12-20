# Chapter 5 — Rendering Architecture (CBUF, Present Modes, Integer Scaling, Formats, and Passes) (ARCANEE v0.1)

## 5.1 Overview

ARCANEE v0.1 renders cartridges into an internal **Console Framebuffer (CBUF)** at a fixed preset resolution and aspect ratio, then presents that image onto the OS window **backbuffer** using a formally defined **Present Mode** (scaling + viewport policy). The Workbench UI is rendered in **native display/window resolution** and composited above the cartridge image.

This chapter defines:

* Render spaces and coordinate systems
* Console framebuffer presets (16:9 and 4:3)
* Swapchain/backbuffer creation and resizing
* Mandatory texture formats (32bpp) and color space rules
* Render pass order and composition (3D, 2D canvas, present, UI)
* Present modes: `fit`, `integer_nearest`, `fill`, `stretch`
* Pixel-perfect integer scaling rules (viewport math, filtering, and sampling constraints)
* Handling of window resizing, fullscreen desktop, and DPI
* Resource lifetime, synchronization, and frame pacing requirements

---

## 5.2 Rendering Spaces and Coordinate Systems

### 5.2.1 Spaces (Normative)

ARCANEE defines three distinct spaces:

1. **Console Space (Cartridge Space)**

   * Dimensions: `w × h` equal to the active CBUF preset.
   * Origin: top-left.
   * Units: pixels (float values allowed by API; see pixel-perfect rules for integer scaling).

2. **Display Space (Workbench/Window Space)**

   * Dimensions: `W × H` equal to the OS window client size (windowed) or the desktop resolution (fullscreen desktop).
   * Origin: top-left.

3. **Normalized Device Coordinates (NDC)**

   * Internal GPU rendering space for full-screen passes (e.g., present pass).

### 5.2.2 DPI and Pixel Ratio (Normative)

ARCANEE MUST treat `W × H` as **physical framebuffer pixels** (not logical points). If the platform provides a logical-size-to-pixel-size mapping (e.g., high DPI), the runtime MUST use the actual drawable pixel size when creating/resizing the swapchain.

The Workbench UI MAY scale its UI layout using ImGui’s DPI scaling mechanisms, but **CBUF dimensions and present calculations MUST use physical pixels**.

---

## 5.3 Console Framebuffer (CBUF) Presets (Normative)

### 5.3.1 Supported Presets

ARCANEE v0.1 MUST support the following fixed preset resolutions:

**Aspect 16:9**

* Ultra: 3840 × 2160
* High: 1920 × 1080
* Medium: 960 × 540
* Low: 480 × 270

**Aspect 4:3**

* Ultra: 3200 × 2400
* High: 1600 × 1200
* Medium: 800 × 600
* Low: 400 × 300

### 5.3.2 Selection Rules

* The cartridge manifest specifies preferred `aspect` and `preset`.
* If `allow_user_override=true`, the Workbench MAY override them at runtime.
* The active CBUF size MUST be exposed to cartridges via `sys.getConsoleSize()`.

### 5.3.3 CBUF as the Cartridge “Screen”

All cartridge rendering (`gfx.*` and `gfx3d.*`) MUST target the active CBUF by default unless an offscreen surface is explicitly bound via `gfx.setSurface()`.

---

## 5.4 Swapchain and Backbuffer (Normative)

### 5.4.1 Backbuffer Dimensions

* Windowed: backbuffer size MUST match the window drawable pixel size (`W × H`).
* Fullscreen desktop: backbuffer size MUST match the desktop resolution of the selected display.

### 5.4.2 Fullscreen Desktop Requirement (Workbench)

In Workbench mode, fullscreen MUST be implemented as desktop fullscreen (borderless) and MUST NOT force a display mode change.

### 5.4.3 Resize Handling

On window resize or display change:

* The runtime MUST recreate or resize swapchain backbuffers.
* The runtime MUST recompute present viewport parameters before the next present pass.
* The runtime MUST preserve cartridge determinism; resizing MUST NOT affect simulation timing semantics (only presentation).

---

## 5.5 Texture Formats, 32bpp Requirement, and Color Space

### 5.5.1 32bpp Requirement (Normative)

CBUF color buffers and 2D canvas upload textures MUST be 32 bits per pixel (**32bpp**).

### 5.5.2 Preferred Format Policy (Normative)

ARCANEE MUST select the “fastest reasonable” 32bpp format per backend, using the following policy:

* Preferred: **BGRA8** (UNORM or SRGB variant)
* Fallback: **RGBA8** (UNORM or SRGB variant)

The runtime MUST apply the selection consistently across:

* swapchain/backbuffer format
* CBUF format
* intermediate textures used for present

If exact matching is not possible, the runtime MUST ensure that any necessary channel swizzles/conversions do not change API-visible results beyond expected color/gamma behavior.

### 5.5.3 Color Space Policy (Normative)

ARCANEE v0.1 SHALL support one of the following operational color policies, chosen by runtime configuration:

**Policy A (Recommended): sRGB backbuffer**

* The swapchain/backbuffer is created in an sRGB-capable 32bpp format where supported.
* Internally, shading is performed in linear space; output to the backbuffer is encoded to sRGB by the GPU render target semantics.

**Policy B: Linear UNORM backbuffer**

* The swapchain/backbuffer uses UNORM; the runtime manages gamma as needed.

ARCANEE MUST document which policy is active. In Workbench mode, Policy A is RECOMMENDED.

### 5.5.4 Alpha Semantics

CBUF is treated as premultiplied or straight alpha consistently. v0.1 requirement:

* The runtime MUST choose one alpha convention and apply it consistently for:

  * ThorVG surface composition
  * sprite/image drawing
  * UI overlay
* The convention MUST be documented for developers (recommended: premultiplied alpha for easier composition).

---

## 5.6 Render Pass Topology (Normative)

### 5.6.1 Required Pass Order

Per video frame, the runtime MUST execute the following passes in order (when cartridge is Running):

1. **3D Pass** → render 3D scene into CBUF (color + optional depth)
2. **2D Canvas Pass** → composite 2D vector canvas results over CBUF
3. **Present Pass** → scale CBUF into the backbuffer according to Present Mode
4. **Workbench UI Pass** → render ImGui (Workbench) in native display space over the backbuffer
5. **Present Swapchain**

### 5.6.2 Optional Passes (v0.1)

The runtime MAY include:

* Clear passes
* Post-processing passes for 3D (tone mapping, bloom, etc.), provided they do not change the required observable semantics of present modes and CBUF sizing.

---

## 5.7 2D Canvas Integration (ThorVG → GPU) (Normative)

### 5.7.1 Rendering Model

In v0.1, ThorVG rendering MAY be implemented as:

* **CPU rasterization to a 32bpp surface** (recommended baseline)
* followed by **GPU texture upload** and composition

### 5.7.2 Upload Requirements

The runtime MUST ensure:

* The uploaded texture matches CBUF size (or the active surface size for offscreen rendering).
* Updates are synchronized safely:

  * The runtime MUST NOT overwrite a texture region currently in use by the GPU.
  * The runtime SHOULD use double-buffering or ring-buffered staging allocations to avoid stalls.

### 5.7.3 Composition Requirements

The ThorVG output MUST be composited over the existing CBUF contents using the selected alpha convention (see §5.5.4).

---

## 5.8 Present Modes (Scaling and Viewport) (Normative)

### 5.8.1 Definitions

Let:

* CBUF dimensions be `w × h`
* backbuffer dimensions be `W × H`

All present modes define a **viewport rectangle** `(vpX, vpY, vpW, vpH)` in display space where the CBUF image is drawn.

The runtime MUST ensure `vpX`, `vpY`, `vpW`, and `vpH` are integers (pixel-aligned).

### 5.8.2 Present Mode: `fit` (Aspect-Preserving Letterbox)

**Goal:** Preserve aspect ratio; show the entire CBUF; letterbox/pillarbox as needed.

Algorithm:

* `s = min(W / w, H / h)`
* `vpW = floor(w * s)`
* `vpH = floor(h * s)`
* `vpX = floor((W - vpW) / 2)`
* `vpY = floor((H - vpH) / 2)`

Filtering:

* Default filter is implementation-defined (Workbench setting), RECOMMENDED:

  * linear filtering for non-integer scaling in modern mode
  * nearest filtering optional for crispness

### 5.8.3 Present Mode: `integer_nearest` (Pixel-Perfect Integer Scaling)

**Goal:** Preserve aspect ratio; scale by an **integer factor** only; enforce **nearest filtering**; center image; avoid subpixel artifacts.

Algorithm:

* `k = floor(min(W / w, H / h))`
* If `k < 1`: fallback to `fit` for that frame (normative).
* Else:

  * `vpW = w * k`
  * `vpH = h * k`
  * `vpX = floor((W - vpW) / 2)`
  * `vpY = floor((H - vpH) / 2)`

Filtering (normative):

* Minification/magnification MUST be **nearest/point**.
* Mipmaps MUST NOT be used for the CBUF present texture in this mode.

Sampling (normative):

* The present shader/pipeline MUST sample the CBUF texture such that texel centers map to pixel centers. Implementations MUST ensure no half-texel drift occurs across backends.

### 5.8.4 Present Mode: `fill` (Aspect-Preserving Crop)

**Goal:** Preserve aspect ratio; fill the entire backbuffer; crop excess.

Algorithm:

* `s = max(W / w, H / h)`
* `vpW = ceil(w * s)`
* `vpH = ceil(h * s)`
* `vpX = floor((W - vpW) / 2)`
* `vpY = floor((H - vpH) / 2)`

Filtering:

* Linear filtering RECOMMENDED.

### 5.8.5 Present Mode: `stretch` (Non-Aspect-Preserving)

**Goal:** Fill entire backbuffer; do not preserve aspect ratio.

Algorithm:

* `vpX = 0`, `vpY = 0`, `vpW = W`, `vpH = H`

Filtering:

* Implementation-defined; linear RECOMMENDED.

### 5.8.6 Letterbox Color

When `vpW < W` or `vpH < H`, the uncovered backbuffer region MUST be filled with a deterministic clear color:

* Default: opaque black (`0xFF000000`)
* Workbench MAY allow customization; if so, the default MUST remain black.

### 5.8.7 Reference Implementation (Normative)

```c++
// Present mode viewport calculation - NORMATIVE reference implementation

struct Viewport { int x, y, w, h; };

enum class PresentMode { Fit, IntegerNearest, Fill, Stretch };

Viewport calculateViewport(int W, int H,       // Backbuffer
                           int w, int h,       // CBUF
                           PresentMode mode,
                           int* outScale = nullptr) {
    Viewport vp;
    
    switch (mode) {
    case PresentMode::Fit: {
        double s = std::min(double(W) / w, double(H) / h);
        vp.w = std::floor(w * s);
        vp.h = std::floor(h * s);
        vp.x = std::floor((W - vp.w) / 2.0);
        vp.y = std::floor((H - vp.h) / 2.0);
        break;
    }
    
    case PresentMode::IntegerNearest: {
        int k = std::min(W / w, H / h);
        if (k < 1) {
            // Fallback to fit when window too small
            return calculateViewport(W, H, w, h, PresentMode::Fit, nullptr);
        }
        vp.w = w * k;
        vp.h = h * k;
        vp.x = (W - vp.w) / 2;  // Integer division already floors
        vp.y = (H - vp.h) / 2;
        if (outScale) *outScale = k;
        break;
    }
    
    case PresentMode::Fill: {
        double s = std::max(double(W) / w, double(H) / h);
        vp.w = std::ceil(w * s);
        vp.h = std::ceil(h * s);
        vp.x = std::floor((W - vp.w) / 2.0);  // May be negative
        vp.y = std::floor((H - vp.h) / 2.0);
        break;
    }
    
    case PresentMode::Stretch:
        vp = {0, 0, W, H};
        break;
    }
    
    return vp;
}
```

**Example calculations:**

| CBUF | Backbuffer | Mode | Result |
|------|------------|------|--------|
| 480×270 | 1920×1080 | integer_nearest | k=4, vp=(0,0,1920,1080) |
| 480×270 | 1920×1200 | integer_nearest | k=4, vp=(0,60,1920,1080) |
| 480×270 | 1600×900 | integer_nearest | k=3, vp=(80,45,1440,810) |
| 960×540 | 1920×1080 | fit | vp=(0,0,1920,1080) |
| 400×300 | 1920×1080 | fit | vp=(240,0,1440,1080) |

---

## 5.9 Present Pipeline Implementation Requirements (DiligentEngine)

### 5.9.1 Required GPU Objects

ARCANEE MUST provide:

* A render target texture for CBUF (color; optional depth for 3D).
* A present pipeline state object (PSO) capable of rendering a full-screen primitive into the backbuffer with a selectable sampler:

  * `SamplerPointClamp` for `integer_nearest`
  * `SamplerLinearClamp` for other modes (recommended)
* A uniform/constant buffer containing viewport-to-UV mapping parameters (or equivalent push constants strategy).

### 5.9.2 Full-Screen Primitive

The present pass SHOULD use a full-screen triangle to avoid cracks and minimize vertex overhead. If using a quad, it MUST avoid precision issues and must map UVs consistently.

### 5.9.3 Resource State and Synchronization (Normative)

The runtime MUST ensure:

* CBUF is transitioned to shader-readable state before sampling in the present pass.
* Backbuffer is transitioned to render-target state before drawing the present and UI passes.
* Any uploaded ThorVG texture is transitioned appropriately before composition.

The exact state names are backend-specific, but the implementation MUST guarantee correctness and avoid data hazards.

---

## 5.10 Frame Pacing and VSync

### 5.10.1 Modes

ARCANEE SHOULD support:

* VSync (default in Workbench)
* Unlocked (optional; may be used for benchmarking)

### 5.10.2 Determinism Constraint

VSync or refresh rate MUST NOT alter the fixed timestep update rate or the `dt` passed to `update()`.

---

## 5.11 Offscreen Surfaces and Multi-Target Rendering (v0.1)

### 5.11.1 Surfaces

The `gfx.createSurface(w,h)` API creates an offscreen 32bpp render target usable for 2D operations.

* Surfaces MUST be independent of CBUF and MUST NOT implicitly resize.

### 5.11.2 Surface Binding

* `gfx.setSurface(surfaceHandle)` directs subsequent `gfx.*` rendering to that surface.
* `gfx.setSurface(null)` restores rendering to CBUF.

### 5.11.3 Presenting Surfaces

v0.1 does not require presenting arbitrary surfaces directly; surfaces are intended for composition and caching. Cartridges may draw surfaces into CBUF via image draw operations if exposed by the runtime.

---

## 5.12 Windowing and Display Changes

### 5.12.1 Multi-Monitor

In Workbench mode, fullscreen desktop MUST apply to the currently selected display.

* The runtime SHOULD allow selecting the target display.
* Moving a fullscreen desktop window between displays MAY be constrained by the OS; behavior MUST be documented.

### 5.12.2 Device Loss / Swapchain Recreate

If the graphics backend requires device loss handling:

* The runtime MUST recreate swapchain and dependent resources safely.
* Cartridge simulation MUST pause or continue per policy, but must not crash the host.

---

## 5.13 Pixel-Perfect Guidance (Developer-Facing, Normative/Informative Split)

### 5.13.1 Normative Guarantees

When `present.mode = integer_nearest` and `k >= 1`, ARCANEE guarantees:

* Integer scaling factor `k` is used.
* Nearest filtering is used.
* Viewport is pixel-aligned and centered.

### 5.13.2 Informative Recommendations to Cartridge Authors

To achieve best results:

* Draw sprites/text aligned to integer pixel coordinates in console space.
* Avoid subpixel transforms unless intentionally producing smooth motion (which may appear “jittery” under integer scaling).
* Prefer low/medium presets for strong pixel-art aesthetics.

(These recommendations do not change runtime requirements.)

---

## 5.14 Compliance Checklist (Chapter 5)

An implementation is compliant with Chapter 5 if:

1. It implements a CBUF at one of the fixed preset resolutions and exposes its size to scripts.
2. It renders cartridge output into CBUF and composites Workbench UI in native display space.
3. It implements the required pass order: 3D → 2D → Present → UI → swapchain present.
4. It implements all present modes (`fit`, `integer_nearest`, `fill`, `stretch`) with the exact viewport math specified.
5. In `integer_nearest`, it enforces nearest filtering, disallows mipmaps for the sampled CBUF, and uses integer scaling factor `k`.
6. It handles window resize and swapchain recreation deterministically without affecting simulation timestep.
7. It uses 32bpp textures for CBUF and 2D surfaces and applies the preferred format policy (BGRA8 preferred, RGBA8 fallback).
8. It documents and consistently applies a single alpha convention and color space policy.