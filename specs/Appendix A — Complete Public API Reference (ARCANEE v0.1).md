# Appendix A — Complete Public API Reference (ARCANEE v0.1)

## A.1 Scope

This appendix consolidates the **entire cartridge-facing public API** defined by ARCANEE v0.1 into a single reference listing. The semantics and constraints are defined in Chapters 4–11; this appendix focuses on **names, signatures, return conventions, and normative notes**.

### A.1.1 Conventions

* All APIs are available to cartridges as global tables: `sys`, `fs`, `inp`, `gfx`, `gfx3d`, `audio`, and (Dev Mode only) `dev`, `app`.
* **Handle types** are integers. `0` is always an invalid handle.
* Failure returns follow Chapter 11:

  * handle-returning → `0`
  * boolean-returning → `false`
  * table-returning → `null`
  * void-returning → no-op + set last error (Dev Mode SHOULD also log)
* `sys.getLastError()` returns a cartridge-scoped human-readable error string; success does not clear it.

---

## A.2 `sys.*` — System and Runtime

### A.2.1 Versions and Timing

* `sys.getApiVersion() -> string`
* `sys.getEngineVersion() -> string`
* `sys.getFrameCount() -> int`
* `sys.getTickHz() -> int`
* `sys.getDeltaFixed() -> float`
  **Normative:** must equal `1.0 / sys.getTickHz()`.

### A.2.2 Display and Console Buffer

* `sys.getDisplaySize() -> (int W, int H)`
  **Normative:** physical drawable pixels.
* `sys.getConsoleSize() -> (int w, int h)`
* `sys.getAspectMode() -> string`  // `"16:9"` or `"4:3"`
* `sys.getPreset() -> string`      // `"low"|"medium"|"high"|"ultra"`

### A.2.3 Presentation / Scaling

* `sys.getScalingMode() -> string` // `"fit"|"integer_nearest"|"fill"|"stretch"`
* `sys.setScalingMode(mode: string) -> bool`
* `sys.setConsoleMode(aspect: string, preset: string) -> bool`
  **Normative:** allowed only when manifest permits user override or in Dev Mode policy.

### A.2.4 Deterministic RNG and Time

* `sys.rand() -> int`
* `sys.srand(seed: int) -> void`
* `sys.timeMs() -> int`
  **Normative:** monotonic time; non-deterministic; should not be used for simulation decisions.

### A.2.5 Logging and Errors

* `sys.log(msg: string) -> void`
* `sys.warn(msg: string) -> void`
* `sys.error(msg: string) -> void`
* `sys.getLastError() -> string|null`
* `sys.clearLastError() -> void`

---

## A.3 `fs.*` — Sandboxed Virtual Filesystem (PhysFS-backed)

**All paths MUST be canonical VFS paths:** `cart:/...`, `save:/...`, `temp:/...`. Paths without namespace MUST be rejected.

### A.3.1 Text I/O (UTF-8)

* `fs.readText(path: string) -> string|null`
* `fs.writeText(path: string, text: string) -> bool`

### A.3.2 Binary I/O

* `fs.readBytes(path: string) -> array<int>|null`  // 0..255
* `fs.writeBytes(path: string, data: array<int>) -> bool`

### A.3.3 Filesystem Operations

* `fs.exists(path: string) -> bool`
* `fs.listDir(path: string) -> array<string>|null`
  **Normative:** lexicographically sorted stable output.
* `fs.mkdir(path: string) -> bool`
* `fs.remove(path: string) -> bool`
* `fs.stat(path: string) -> table|null`
  Returns `{ type="file|dir", size=int, mtime=int|null }`.

---

## A.4 `inp.*` — Deterministic Input

### A.4.1 Keyboard

* `inp.keyDown(code: int) -> bool`
* `inp.keyPressed(code: int) -> bool`
* `inp.keyReleased(code: int) -> bool`

### A.4.2 Mouse

* `inp.mousePos() -> (int x, int y)`
  **Normative:** returns console-space coords; returns `(-1,-1)` outside the CBUF viewport.
* `inp.mouseDown(btn: int) -> bool`
* `inp.mousePressed(btn: int) -> bool`
* `inp.mouseReleased(btn: int) -> bool`
* `inp.mouseWheel() -> (float dx, float dy)`
  **Normative:** accumulates within tick and resets each tick.

### A.4.3 Gamepad

* `inp.padCount() -> int`
* `inp.padButtonDown(pad: int, btn: int) -> bool`
* `inp.padButtonPressed(pad: int, btn: int) -> bool`
* `inp.padButtonReleased(pad: int, btn: int) -> bool`
* `inp.padAxis(pad: int, axis: int) -> float`  // [-1,+1]

---

## A.5 `gfx.*` — 2D Canvas API (ThorVG-backed)

### A.5.1 Targets / Surfaces

* `gfx.createSurface(w: int, h: int) -> int`          // SurfaceHandle
* `gfx.freeSurface(s: int) -> void`
* `gfx.setSurface(s: int|null) -> void`               // null = CBUF
* `gfx.getTargetSize() -> (int w, int h)`
* `gfx.clear(color: u32=0x00000000) -> void`

### A.5.2 State Stack and Transform

* `gfx.save() -> void`
* `gfx.restore() -> void`
* `gfx.resetTransform() -> void`
* `gfx.setTransform(a: float,b: float,c: float,d: float,e: float,f: float) -> void`
* `gfx.translate(x: float, y: float) -> void`
* `gfx.rotate(rad: float) -> void`
* `gfx.scale(x: float, y: float) -> void`

### A.5.3 Global Compositing

* `gfx.setGlobalAlpha(a: float) -> void`
* `gfx.setBlend(mode: string) -> bool`

### A.5.4 Stroke/Fill Styles

* `gfx.setFillColor(color: u32) -> void`
* `gfx.setStrokeColor(color: u32) -> void`
* `gfx.setLineWidth(px: float) -> void`
* `gfx.setLineJoin(join: string) -> bool`         // miter|round|bevel
* `gfx.setLineCap(cap: string) -> bool`           // butt|round|square
* `gfx.setMiterLimit(v: float) -> void`
* `gfx.setLineDash(dashes: array<float>|null, offset: float=0) -> bool`

### A.5.5 Path Construction

* `gfx.beginPath() -> void`
* `gfx.closePath() -> void`
* `gfx.moveTo(x: float, y: float) -> void`
* `gfx.lineTo(x: float, y: float) -> void`
* `gfx.quadTo(cx: float, cy: float, x: float, y: float) -> void`
* `gfx.cubicTo(c1x: float, c1y: float, c2x: float, c2y: float, x: float, y: float) -> void`
* `gfx.arc(x: float, y: float, r: float, a0: float, a1: float, ccw: bool=false) -> void`
* `gfx.rect(x: float, y: float, w: float, h: float) -> void`

### A.5.6 Drawing

* `gfx.fill() -> void`
* `gfx.stroke() -> void`
* `gfx.clip() -> void`
* `gfx.resetClip() -> void`
* `gfx.fillRect(x: float, y: float, w: float, h: float) -> void`
* `gfx.strokeRect(x: float, y: float, w: float, h: float) -> void`
* `gfx.clearRect(x: float, y: float, w: float, h: float) -> void`

### A.5.7 Images

* `gfx.loadImage(path: string) -> int`            // ImageHandle
* `gfx.freeImage(img: int) -> void`
* `gfx.getImageSize(img: int) -> (int w, int h)`
* `gfx.drawImage(img: int, dx: float, dy: float) -> void`
* `gfx.drawImageRect(img: int,
                     sx: int, sy: int, sw: int, sh: int,
                     dx: float, dy: float, dw: float, dh: float) -> void`

### A.5.8 Paint Objects (Gradients)

* `gfx.createLinearGradient(x1: float, y1: float, x2: float, y2: float) -> int` // PaintHandle
* `gfx.createRadialGradient(cx: float, cy: float, r: float) -> int`             // PaintHandle
* `gfx.paintSetStops(p: int, stops: array<table>) -> bool`
* `gfx.paintSetSpread(p: int, mode: string) -> bool`  // pad|repeat|reflect
* `gfx.paintFree(p: int) -> void`
* `gfx.setFillPaint(p: int|null) -> void`
* `gfx.setStrokePaint(p: int|null) -> void`

### A.5.9 Text

* `gfx.loadFont(path: string, sizePx: int, opts: table=null) -> int` // FontHandle
* `gfx.freeFont(font: int) -> void`
* `gfx.setFont(font: int) -> void`
* `gfx.setTextAlign(align: string) -> bool`      // left|center|right|start|end
* `gfx.setTextBaseline(base: string) -> bool`    // top|middle|alphabetic|bottom
* `gfx.measureText(text: string) -> table|null`
* `gfx.fillText(text: string, x: float, y: float, maxWidth: float=null) -> void`
* `gfx.strokeText(text: string, x: float, y: float, maxWidth: float=null) -> void`

---

## A.6 `gfx3d.*` — 3D Scene API

### A.6.1 Scene Control

* `gfx3d.createScene() -> int`            // SceneHandle
* `gfx3d.destroyScene(scene: int) -> void`
* `gfx3d.setActiveScene(scene: int) -> bool`
* `gfx3d.getActiveScene() -> int`

### A.6.2 Entities and Transforms

* `gfx3d.createEntity(name: string="") -> int`   // EntityId
* `gfx3d.destroyEntity(e: int) -> void`
* `gfx3d.setEntityName(e: int, name: string) -> void`
* `gfx3d.getEntityName(e: int) -> string|null`
* `gfx3d.setTransform(e: int, pos: table, rot: table, scale: table) -> bool`
* `gfx3d.getTransform(e: int) -> table|null`

### A.6.3 Mesh Renderers

* `gfx3d.addMeshRenderer(e: int, mesh: int, material: int) -> bool`
* `gfx3d.removeMeshRenderer(e: int) -> void`
* `gfx3d.setVisible(e: int, visible: bool) -> void`

### A.6.4 Cameras

* `gfx3d.createCamera() -> int`                     // CameraHandle
* `gfx3d.destroyCamera(cam: int) -> void`
* `gfx3d.setCameraPerspective(cam: int, fovY: float, near: float, far: float) -> bool`
* `gfx3d.setCameraOrthographic(cam: int, height: float, near: float, far: float) -> bool` // optional
* `gfx3d.setCameraView(cam: int, eye: table, at: table, up: table) -> bool`
* `gfx3d.attachCamera(e: int, cam: int) -> bool`    // optional
* `gfx3d.setActiveCamera(cam: int) -> bool`
* `gfx3d.getActiveCamera() -> int`

### A.6.5 Lights

* `gfx3d.createLight(type: string) -> int`          // LightHandle
* `gfx3d.destroyLight(light: int) -> void`
* `gfx3d.attachLight(e: int, light: int) -> bool`
* `gfx3d.detachLight(e: int) -> void`
* `gfx3d.setLightParams(light: int, params: table) -> bool`

### A.6.6 Assets: glTF

* `gfx3d.loadGLTF(path: string, opts: table=null) -> table|null`
  Returns `{ scene, root, meshes, materials, textures, animations }`.

### A.6.7 Textures

* `gfx3d.loadTexture(path: string, opts: table=null) -> int`   // TextureHandle
* `gfx3d.destroyTexture(t: int) -> void`
* `gfx3d.getTextureSize(t: int) -> (int w, int h)`

### A.6.8 Materials (PBR)

* `gfx3d.createMaterial() -> int`                  // MaterialHandle
* `gfx3d.destroyMaterial(m: int) -> void`
* `gfx3d.matSetBaseColor(m: int, color: table) -> bool`        // {r,g,b,a}
* `gfx3d.matSetBaseColorTex(m: int, tex: int|null) -> bool`
* `gfx3d.matSetMetallicRoughness(m: int, metallic: float, roughness: float) -> bool`
* `gfx3d.matSetMetallicRoughnessTex(m: int, tex: int|null) -> bool`
* `gfx3d.matSetNormalTex(m: int, tex: int|null, scale: float=1.0) -> bool`
* `gfx3d.matSetEmissive(m: int, color: table) -> bool`         // {r,g,b}
* `gfx3d.matSetEmissiveTex(m: int, tex: int|null) -> bool`
* `gfx3d.matSetAlphaMode(m: int, mode: string, cutoff: float=0.5) -> bool`  // opaque|mask|blend
* `gfx3d.matSetDoubleSided(m: int, on: bool) -> bool`

### A.6.9 Environment and Post

* `gfx3d.setEnvironmentHDR(path: string, opts: table=null) -> bool`
* `gfx3d.clearEnvironment() -> void`
* `gfx3d.setTonemapper(mode: string) -> bool`      // aces|reinhard|none
* `gfx3d.enableTAA(on: bool) -> bool`              // optional
* `gfx3d.setBloom(params: table) -> bool`          // optional
* `gfx3d.setSSAO(params: table) -> bool`           // optional

### A.6.10 Render

* `gfx3d.render() -> bool`

---

## A.7 `audio.*` — Audio (libopenmpt Modules + SFX)

### A.7.1 Modules (Music)

* `audio.loadModule(path: string) -> int`      // ModuleHandle
* `audio.freeModule(m: int) -> void`
* `audio.playModule(m: int, loop: bool=true) -> bool`
* `audio.stopModule() -> void`
* `audio.pauseModule() -> void`
* `audio.resumeModule() -> void`
* `audio.setModuleVolume(v: float) -> void`
* `audio.setModuleTempo(factor: float) -> bool`        // recommended 0.5..2.0
* `audio.seekModuleSeconds(t: float) -> bool`
* `audio.getModuleInfo(m: int) -> table|null`

### A.7.2 SFX (Samples)

* `audio.loadSound(path: string) -> int`       // SoundHandle
* `audio.freeSound(s: int) -> void`
* `audio.playSound(s: int, vol: float=1.0, pan: float=0.0, pitch: float=1.0) -> int` // VoiceId
* `audio.stopVoice(voice: int) -> void`
* `audio.setVoiceVolume(voice: int, vol: float) -> void`
* `audio.setVoicePan(voice: int, pan: float) -> void`
* `audio.setVoicePitch(voice: int, pitch: float) -> void`

### A.7.3 Master

* `audio.setMasterVolume(v: float) -> void`
* `audio.getMasterVolume() -> float`
* `audio.stopAll() -> void`

---

## A.8 `dev.*` — Development Tools (Dev Mode Only)

**Normative:** `dev.*` MUST be unavailable or no-op in Player mode.

* `dev.reloadCartridge() -> void`
* `dev.captureFrame(path: string) -> bool`          // writes to save:/ or temp:/ only
* `dev.profileBegin(name: string) -> void`
* `dev.profileEnd(name: string) -> void`

(Additional internal debugging functions may exist, but must not be relied upon by cartridges in v0.1.)

---

## A.9 `app.*` — Workbench/Window Control (Workbench Only)

These calls are intended for Workbench-controlled behavior and MUST be restricted in Player mode.

* `app.isFullscreen() -> bool`
* `app.setFullscreen(on: bool) -> void`

---

## A.10 Required Cartridge Entry Points (Global)

Cartridges MUST define these functions in the global environment:

* `function init() { }`
* `function update(dt) { }`
* `function draw(alpha) { }`

---

## A.11 Notes on Forward Compatibility (Informative)

* APIs marked “optional” in Chapters 6–8 still appear in this appendix to preserve a stable ABI; if not implemented, they must fail safely with `UnsupportedFeature`.
* Future versions may add new namespaces (e.g., `net.*`), but v0.1 must keep these names reserved.

---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
