# Appendix E — Rendering Formats, Alpha, and Color Space Policy (Normative Choices and Rationale) (ARCANEE v0.1)

## E.1 Scope

This appendix defines the **normative rendering format and color management policy** for ARCANEE v0.1, including:

* required 32bpp formats and selection rules
* alpha convention (premultiplied vs straight) and how each subsystem must comply
* color space handling (linear vs sRGB) and tonemapping boundaries
* rules for 2D (ThorVG) → GPU upload and compositing
* rules for 3D (PBR) → CBUF output and post-processing
* rules for the present pass and Workbench UI overlay

This appendix is normative: implementations MUST follow it unless they explicitly declare a documented deviation profile.

---

## E.2 Normative Baseline Policy (ARCANEE v0.1)

ARCANEE v0.1 SHALL operate under the following baseline policy:

1. **Color rendering space:** Linear (floating-point math), with **sRGB output encoding** on the final backbuffer where supported.
2. **CBUF and 2D surfaces:** 32bpp UNORM textures in a backend-optimal channel order (BGRA preferred, RGBA fallback).
3. **Alpha convention:** **Premultiplied alpha** for all compositing operations in 2D and between 2D/3D layers.
4. **Backbuffer format:** **sRGB 32bpp** where supported; otherwise UNORM with explicit gamma encoding in present shader.

These choices are designed to:

* simplify correct compositing of semi-transparent 2D UI/vector art
* reduce color banding and gamma mistakes
* preserve “modern” color correctness while still allowing retro presentation modes

---

## E.3 Texture Format Requirements (32bpp)

### E.3.1 Required Supported Formats

ARCANEE v0.1 MUST support at minimum:

* `RGBA8_UNORM` and/or `BGRA8_UNORM` for render targets and textures
* `RGBA8_UNORM_SRGB` and/or `BGRA8_UNORM_SRGB` for sRGB backbuffer when supported by the backend

The exact enum names are backend-specific; this is a conceptual requirement.

### E.3.2 Format Selection Policy (Normative)

The runtime MUST implement the following selection order:

1. Prefer `BGRA8_*` formats for the swapchain/backbuffer if supported by the active backend.
2. Otherwise use `RGBA8_*`.
3. CBUF MUST match backbuffer channel ordering where feasible to minimize swizzle and conversion.
4. 2D upload textures SHOULD match CBUF channel ordering.

If any format is unavailable:

* the runtime MUST choose the nearest equivalent 32bpp format
* and MUST document the selection in Workbench diagnostics

### E.3.3 sRGB vs UNORM Choice (Normative)

* Backbuffer SHOULD be `*_SRGB` where supported.
* If the backend does not support sRGB swapchain/backbuffer formats, the runtime MUST:

  * render the final output into a UNORM backbuffer and
  * apply sRGB encoding in the present shader (see §E.6.4).

---

## E.4 Alpha Convention (Premultiplied) (Normative)

### E.4.1 Definition

A color `(r,g,b,a)` is **premultiplied** if the stored RGB are already multiplied by alpha:

* `rgb_premul = rgb_linear * a`

### E.4.2 Platform-Wide Requirement

ARCANEE v0.1 MUST treat all composited layers as premultiplied alpha:

* 2D canvas output is premultiplied
* 2D images must be converted to premultiplied alpha at load time or draw time
* 3D output must be composed with 2D using premultiplied blending rules

### E.4.3 Blending State Requirements

All compositing passes using alpha MUST use blend factors consistent with premultiplied alpha:

* **Color blend**:

  * `src = ONE`
  * `dst = ONE_MINUS_SRC_ALPHA`
* **Alpha blend**:

  * `src = ONE`
  * `dst = ONE_MINUS_SRC_ALPHA`

(Exact blend state configuration depends on backend; the logical result must match.)

### E.4.4 Implications for Asset Loading (Normative)

* Images (PNG etc.) are typically stored with straight alpha. ARCANEE MUST convert image pixel data to premultiplied alpha before rasterization/compositing in the canvas pipeline.
* If conversion is performed on the GPU, it must be deterministic and consistent with CPU conversion.

### E.4.5 ThorVG Output Alignment

The 2D backend must deliver output compatible with premultiplied alpha compositing.

* If ThorVG produces straight-alpha output, ARCANEE MUST premultiply it prior to compositing.
* If ThorVG already produces premultiplied output, ARCANEE MUST not double-premultiply.

The runtime MUST document the chosen approach and verify it via tests (see §E.8).

---

## E.5 Color Space Policy (Linear Workflow) (Normative)

### E.5.1 Linear Internal Space

All lighting and blending calculations SHOULD occur in linear space:

* 3D PBR shading is performed in linear space.
* 2D blending should conceptually occur in linear space; however, v0.1 may accept that some 2D CPU rasterization occurs in an approximation. The output must remain stable and visually correct to reasonable tolerance.

### E.5.2 sRGB Textures

Texture sampling must respect color space:

* “Color” textures (albedo/baseColor, emissive, UI images) SHOULD be treated as sRGB at sampling time.
* “Data” textures (normal maps, metallic-roughness, occlusion) MUST be treated as linear.

This requires the runtime to:

* tag textures with an `srgb` flag at load time
* create appropriate GPU views or sampling conversions

### E.5.3 CBUF Output Space

CBUF is a render target; it represents the composited cartridge image in a linear workflow.

* If CBUF format is UNORM (not SRGB), the runtime MUST ensure the shader output and blending semantics are consistent.
* In v0.1, the recommended model is:

  * keep CBUF as `*_UNORM` (linear-ish storage)
  * treat the final present pass as the point of sRGB encoding (if the backbuffer is not sRGB)

If CBUF is `*_SRGB` (allowed), then rendering into it must account for sRGB render target semantics. This is more error-prone; v0.1 RECOMMENDS keeping CBUF UNORM and letting the backbuffer handle sRGB.

**Normative baseline v0.1 choice:**

* CBUF MUST be `*_UNORM` (not SRGB).
* Backbuffer SHOULD be `*_SRGB`.

---

## E.6 Render Pipeline Boundaries (Normative)

### E.6.1 3D → CBUF

* 3D renderer writes into CBUF color (UNORM) and optional depth.
* If post-processing is enabled (tone mapping etc.), it must be applied before the 2D overlay is composited, unless explicitly documented otherwise.

### E.6.2 2D Canvas → CBUF

* 2D output is generated as a 32bpp surface and uploaded to a GPU texture.
* Composition of 2D over CBUF MUST use premultiplied alpha blending (see §E.4.3).
* Blend modes beyond normal (multiply/screen/overlay/etc.) must be implemented consistently in the 2D backend (ThorVG) or via a shader path, but must remain stable.

### E.6.3 Workbench UI Overlay

Workbench UI (ImGui) is drawn in display space over the backbuffer.

* ImGui rendering must use premultiplied alpha or must be configured to match the engine’s alpha convention.
* The UI should be treated as sRGB-authored; the runtime must ensure it looks correct under the chosen output policy.

### E.6.4 Present Pass and sRGB Encoding

The present pass maps CBUF to backbuffer.

Cases:

**Case A: Backbuffer is sRGB**

* Present shader outputs linear values; the GPU render target performs sRGB encoding automatically.

**Case B: Backbuffer is UNORM (non-sRGB)**

* Present shader MUST apply linear → sRGB encoding before writing to backbuffer.

**Normative:** The runtime MUST choose one case per backend and document it. Workbench should show “Backbuffer: sRGB” or “Backbuffer: UNORM (shader gamma)”.

---

## E.7 Integer Scaling and Filtering Interactions (Normative)

### E.7.1 CBUF Sampling

When presenting CBUF:

* `integer_nearest` mode MUST use nearest/point sampling.
* other modes MAY use linear sampling by default.

### E.7.2 Gamma and Filtering

Filtering in non-linear space can cause artifacts. Under the baseline policy:

* If backbuffer is sRGB and the present pass samples CBUF (UNORM) and writes linear output, filtering occurs in the shader’s numeric domain. This is not a perfect color-managed pipeline but is acceptable in v0.1.
* The runtime should avoid applying additional gamma conversions prior to filtering.

**Normative requirement:** The chosen pipeline must be stable and documented; integer_nearest must remain pixel-perfect and unaffected by gamma conversions.

---

## E.8 Validation and Test Requirements (Normative)

ARCANEE v0.1 implementations MUST include tests to validate:

1. **Premultiplied alpha correctness**

   * Render a semi-transparent white square over black; verify expected composited result (no dark fringes).
   * Render a semi-transparent colored edge and verify no halo typical of straight-alpha misuse.

2. **sRGB correctness (best effort)**

   * Render a mid-gray patch with known linear value and verify output luminance matches expected within tolerance.
   * Ensure baseColor textures flagged sRGB look consistent across backends.

3. **Format/channel ordering**

   * Render distinct R/G/B test pixels and verify they appear correctly (no swapped channels) for BGRA/RGBA selection.

4. **Present mode interactions**

   * Ensure integer_nearest uses point sampling (no blur) and correct scaling `k`.

---

## E.9 Deviation Profiles (Optional, Must Be Declared)

If an implementation cannot follow baseline policy due to platform constraints, it MUST declare a deviation profile with:

* backbuffer format choice
* CBUF format choice
* alpha convention
* sRGB handling method

Workbench MUST display the active deviation profile.

Examples:

* **Profile P1:** Backbuffer UNORM + shader gamma, CBUF UNORM, premultiplied alpha
* **Profile P2:** Backbuffer sRGB, CBUF UNORM, premultiplied alpha (baseline)

---

## E.10 Compliance Checklist (Appendix E)

An implementation is compliant with Appendix E if it:

1. Uses 32bpp formats for CBUF and 2D surfaces, with BGRA preferred and RGBA fallback.
2. Implements premultiplied alpha consistently across 2D, 3D composition, and UI overlay.
3. Implements a linear workflow with sRGB output encoding at the backbuffer (or shader gamma fallback).
4. Tags textures as sRGB vs linear appropriately (color vs data textures).
5. Provides validation tests for alpha correctness, channel ordering, and present mode filtering.