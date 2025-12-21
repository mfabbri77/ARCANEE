# ARCANEE v0.1 Development Tasks

## Current Phase: Phase 3 - Rendering Core

---

## Phase 1: Core Foundation Layer ✓
- [x] **1.1 Platform Layer (SDL2)**
  - [x] 1.1.1 SDL2 initialization (Platform.cpp)
  - [x] 1.1.2 Window creation with DPI awareness (Window.cpp)
  - [x] 1.1.3 Desktop fullscreen (borderless) (Window.cpp)
  - [x] 1.1.4 High-resolution timing (Time.cpp)
  - [x] 1.1.5 Fixed timestep main loop (Runtime.cpp)

- [x] **1.2 Virtual Filesystem (PhysFS)**
  - [x] 1.2.1 PhysFS initialization (VfsImpl.cpp)
  - [x] 1.2.2 Path normalization and security (Vfs.cpp)
  - [x] 1.2.3 Namespace enforcement (VfsImpl.cpp)
  - [x] 1.2.4 Text I/O with UTF-8 validation (VfsImpl.cpp)
  - [x] 1.2.5 Binary I/O (VfsImpl.cpp)
  - [x] 1.2.6 Directory operations (VfsImpl.cpp)
  - [x] 1.2.7 Quota enforcement (VfsImpl.cpp)


- [x] **1.3 Manifest Parser**
  - [x] 1.3.1 TOML parser (minimal implementation in Manifest.cpp)
  - [x] 1.3.2 Required field validation (Manifest.cpp)
  - [x] 1.3.3 Display settings parsing (Manifest.cpp)
  - [x] 1.3.4 Permissions parsing (Manifest.cpp)
  - [x] 1.3.5 Caps parsing (Manifest.cpp)

---

## Phase 2: Scripting Engine
- [x] **2.1 Squirrel VM Integration**
  - [x] 2.1.1 VM creation/destruction
  - [x] 2.1.2 Error handler with stack traces
  - [x] 2.1.3 Print/log callbacks
  - [x] 2.1.4 Debug metadata configuration

- [x] **2.2 Module Loader**
  - [x] 2.2.1 Entry script loading
  - [x] 2.2.2 Canonical source names
  - [x] 2.2.3 require() implementation
  - [x] 2.2.4 Module caching

- [x] **2.3 Cartridge Lifecycle**
  - [x] 2.3.1 State machine implementation
  - [x] 2.3.2 Callback dispatch (init/update/draw)
  - [x] 2.3.3 Entry point validation
  - [x] 2.3.4 Fault handling

- [x] **2.4 API Binding Framework**
  - [x] 2.4.1 Binding infrastructure
  - [x] 2.4.2 sys.* bindings
  - [x] 2.4.3 fs.* bindings
  - [x] 2.4.4 Handle pool implementation

- [x] **2.5 Budget Enforcement**
  - [x] 2.5.1 CPU time budget per update (Warning only)
  - [x] 2.5.2 Overrun detection and warning
  - [x] 2.5.3 Hang watchdog (Verified)

---

## Phase 3: Rendering Core
- [x] **3.1 DiligentEngine Setup**
  - [x] 3.1.1 Device creation
  - [x] 3.1.2 Swapchain management
  - [x] 3.1.3 Backend selection
  - [x] 3.1.4 Device loss recovery (stub for v0.1)

- [x] **3.2 Console Framebuffer (CBUF)**
  - [x] 3.2.1 CBUF creation for preset (960×540 16:9)
  - [x] 3.2.2 Framebuffer class with color/depth textures
  - [x] 3.2.3 Clear operations

- [x] **3.3 Present Pass**
  - [x] 3.3.1 Viewport calculation per spec §5.8
  - [x] 3.3.2 Present modes (Fit, IntegerNearest, Fill, Stretch)
  - [x] 3.3.3 Fullscreen triangle shader
  - [x] 3.3.4 Point/Linear samplers
  - [x] 3.3.5 Letterbox clearing

- [x] **3.4 Render Pass Topology**
  - [x] 3.4.1 Pass scheduler (RenderGraph.h)
  - [x] 3.4.2 Resource transitions (via Diligent)
  - [x] 3.4.3 VSync control (setVSync)

---

## Phase 4: 2D Canvas Engine
- [x] **4.1 ThorVG Integration**
  - [x] 4.1.1 ThorVG initialization (custom CMake wrapper)
  - [x] 4.1.2 CPU surface management (SwCanvas ARGB8888)
  - [x] 4.1.3 GPU texture upload (UpdateTexture)
  - [x] 4.1.4 Canvas2D class with beginFrame/endFrame

- [x] **4.2 Command Buffer**
  - [x] 4.2.1 State-based drawing via ThorVG Shape API
  - [x] 4.2.2 GfxBinding Squirrel integration

- [x] **4.3 Canvas State**
  - [x] 4.3.1 State stack (save/restore)
  - [x] 4.3.2 Transform state (translate/rotate/scale)
  - [x] 4.3.3 Style state (fill/stroke colors, line width)
  - [x] 4.3.4 Clip state (basic)

- [x] **4.4 Path Operations**
  - [x] 4.4.1 Path construction (beginPath, moveTo, lineTo, rect)
  - [x] 4.4.2 Quadratic to cubic conversion
  - [x] 4.4.3 Arc (uses appendCircle fallback)
  - [x] 4.4.4 Fill and stroke

- [x] **4.5 Images and Text**
  - [x] 4.5.1 Image loading (tvg::Picture)
  - [x] 4.5.2 Image drawing (drawImage, drawImageRect)
  - [x] 4.5.3 Font loading (tvg::Text)
  - [x] 4.5.4 Text measurement (TODO)
  - [x] 4.5.5 Text rendering (fillText, strokeText)

- [x] **4.6 Advanced Features**
  - [x] 4.6.1 Gradients (LinearGradient, RadialGradient)
  - [x] 4.6.2 Blend modes (setBlend: normal|multiply|screen|etc.)
  - [ ] 4.6.3 Offscreen surfaces (TODO: createSurface)

- [x] **4.7 gfx.* Bindings**
  - [x] 4.7.1 All gfx.* API functions (30+ bindings)
  - [x] 4.7.2 Handle management (images, fonts, paints)

---

## Phase 5: 3D Scene Engine
- [ ] **5.1 Scene Graph**
  - [ ] 5.1.1 Scene container
  - [ ] 5.1.2 Entity system
  - [ ] 5.1.3 Transform component
  - [ ] 5.1.4 Mesh renderer

- [ ] **5.2 Camera System**
  - [ ] 5.2.1 Camera creation
  - [ ] 5.2.2 Perspective projection
  - [ ] 5.2.3 View matrix
  - [ ] 5.2.4 Active camera

- [ ] **5.3 Lighting**
  - [ ] 5.3.1 Light types
  - [ ] 5.3.2 Light parameters
  - [ ] 5.3.3 Light attachment

- [ ] **5.4 glTF Pipeline**
  - [ ] 5.4.1 glTF parser
  - [ ] 5.4.2 Mesh creation
  - [ ] 5.4.3 Material creation
  - [ ] 5.4.4 Texture loading
  - [ ] 5.4.5 Entity hierarchy

- [ ] **5.5 PBR Materials**
  - [ ] 5.5.1 Material creation
  - [ ] 5.5.2 Base color
  - [ ] 5.5.3 Metallic/roughness
  - [ ] 5.5.4 Normal mapping
  - [ ] 5.5.5 Alpha modes

- [ ] **5.6 3D Renderer**
  - [ ] 5.6.1 PBR shaders
  - [ ] 5.6.2 Constant buffers
  - [ ] 5.6.3 Opaque pass
  - [ ] 5.6.4 Transparent pass
  - [ ] 5.6.5 Tonemapping

- [ ] **5.7 gfx3d.* Bindings**
  - [ ] 5.7.1 All gfx3d.* API functions

---

## Phase 6: Audio Engine
- [x] **6.1 Audio Device**
  - [x] 6.1.1 SDL audio initialization (48kHz float32 stereo)
  - [x] 6.1.2 Audio callback
  - [ ] 6.1.3 Device loss handling

- [x] **6.2 Command Queue**
  - [x] 6.2.1 SPSC queue (256 slots)
  - [x] 6.2.2 Command processing (in mixAudio)

- [x] **6.3 Module Player**
  - [x] 6.3.1 Module loading
  - [x] 6.3.2 Module playback
  - [x] 6.3.3 Module controls

- [x] **6.4 SFX Mixer**
  - [x] 6.4.1 Sound loading (Raw working; WAV/OGG decode TODO)
  - [x] 6.4.2 Voice system
  - [x] 6.4.3 Voice mixing
  - [x] 6.4.4 Voice stealing

- [x] **6.5 Master Mixer**
  - [x] 6.5.1 Mix pipeline (AudioManager integration)
  - [x] 6.5.2 Master volume
  - [ ] 6.5.3 Output clipping

- [x] **6.6 audio.* Bindings**
  - [x] 6.6.1 All audio.* API functions

---

## Phase 7: Input System
- [x] **7.1 Input Capture**
  - [x] 7.1.1 Event pump
  - [x] 7.1.2 Input snapshot
  - [x] 7.1.3 Edge detection

- [x] **7.2 Devices**
  - [x] 7.2.1 Keyboard
  - [x] 7.2.2 Mouse
  - [x] 7.2.3 Gamepad
  - [x] 7.2.4 Deadzones (Raw access provided)

- [x] **7.3 Coordinate Mapping**
  - [x] 7.3.1 Display to console space
  - [x] 7.3.2 Out-of-bounds handling

- [ ] **7.4 Focus Management**
  - [ ] 7.4.1 Workbench capture (Requires Phase 8)
  - [ ] 7.4.2 Focus toggle

- [x] **7.5 inp.* Bindings**
  - [x] 7.5.1 All inp.* API functions

---

## Verification & Harmonization
- [ ] **V.1 Runtime Config**
  - [x] CLI arg parsing for cartridge path
  - [x] Remove hardcoded cartridge loading

- [x] **V.2 Test Cartridges (High Quality)**
  - [x] `samples/test_gfx`: Comprehensive primitive showcase (Test Card)
  - [x] `samples/test_input`: Gamepad & Mouse Visualizer
  - [x] `samples/test_audio`: Real XM Module Playback & SFX

- [x] **V.3 Spec Compliance Check**
  - [x] Review Gfx API vs Spec
  - [x] Review Audio API vs Spec
  - [x] Review Input API vs Spec

---

## Phase 8: Workbench IDE
- [ ] **8.1 Text Editor**
- [ ] **8.2 Sprite Editor**
- [ ] **8.3 Map Editor**
- [ ] **8.4 Sound Editor**
- [ ] **8.5 Music Editor**
- [ ] **8.6 Asset Management**

## Debugging Loop (Current)
- [x] Fix Audio Device mix callback (Was empty)
- [x] Fix Audio Manager init (Added volume defaults)
- [x] Fix Palette System (Was using 0x00000000 for palette indices)
  - [x] Add palette to Runtime
  - [x] Add lookup to GfxBinding
  - [x] Initialize PICO-8 palette
- [x] Fix Audio Silence (Missing compile definition and valid data) - **COMPLETE**
  - [x] Verified AudioDevice output (Test Tone)
  - [x] Verified SfxMixer pipeline (Instrumented logs)
  - [x] Confirmed valid assets fixed silence issues
- [x] Fix Black Screen (Canvas2D buffer remains 0) - **FIXED** (Missing setGfxCanvas)

## Phase 8: Workbench IDE
- [ ] **8.1 ImGui Integration**
  - [ ] 8.1.1 ImGui setup
  - [ ] 8.1.2 Docking layout
  - [ ] 8.1.3 Theme

- [ ] **8.2 Project Management**
  - [ ] 8.2.1 Project open/create
  - [ ] 8.2.2 File tree view

- [ ] **8.3 Code Editor**
  - [ ] 8.3.1 TextEditor widget
  - [ ] 8.3.2 Squirrel syntax highlighting
  - [ ] 8.3.3 Undo/redo
  - [ ] 8.3.4 Search

- [ ] **8.4 Run Control**
  - [ ] 8.4.1 Run/Stop/Pause/Reset
  - [ ] 8.4.2 State display
  - [ ] 8.4.3 Hot reload

- [ ] **8.5 Console and Errors**
  - [ ] 8.5.1 Console pane
  - [ ] 8.5.2 Error list
  - [ ] 8.5.3 Stack trace view

- [ ] **8.6 Diagnostics**
  - [ ] 8.6.1 CPU/GPU timing
  - [ ] 8.6.2 Resource counts
  - [ ] 8.6.3 Budget display

- [ ] **8.7 Settings**
  - [ ] 8.7.1 Display settings
  - [ ] 8.7.2 Audio settings

---

## Phase 9: Error Handling
- [ ] Error taxonomy implementation
- [ ] Last error channel
- [ ] Logging system
- [ ] Privacy mode for Player
- [ ] Crash containment

---

## Phase 10: Performance and Compliance
- [ ] **10.1 Budget Enforcement**
  - [ ] CPU budgets
  - [ ] VM memory limits
  - [ ] Canvas limits
  - [ ] Storage quotas
  - [ ] Audio limits

- [ ] **10.2 Compliance Tests**
  - [ ] VFS tests
  - [ ] Tick tests
  - [ ] Present tests
  - [ ] Handle tests
  - [ ] Audio tests

---

## Milestones

- [ ] **M1**: Minimal Viable Runtime (Phase 1-2 core)
- [ ] **M2**: Basic Rendering (Phase 3)
- [ ] **M3**: 2D Graphics (Phase 4)
- [ ] **M4**: Input and Audio (Phase 6-7)
- [ ] **M5**: 3D Graphics (Phase 5)
- [ ] **M6**: Workbench Complete (Phase 8)
- [ ] **M7**: Polish and Compliance (Phase 9-10)

