# Chapter 7B — Extended 3D Scene API Specification (DiligentEngine Retained Model)

## 7B.1 Purpose and Scope

This supplementary chapter provides **detailed implementation-level specifications** for the 3D Scene API, extending Chapter 7 with:

* Complete DiligentEngine integration mapping for ARCANEE `gfx3d.*` functions
* Retained mode rendering model (scene state recording and deferred execution)
* Precise render order specification (3D → 2D → Present)
* Internal architecture and data structures
* Reference pseudocode for implementers

---

## 7B.2 Render Order and Pipeline Architecture (Normative)

### 7B.2.1 Complete Frame Render Order

The ARCANEE rendering pipeline executes in a **strict, non-configurable order** per frame. This order is **normative** and MUST be preserved by all implementations:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                        FRAME RENDER PIPELINE                                  │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  Phase 1: CARTRIDGE update()                                                  │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │ • Execute 0-N update(dt) calls (fixed timestep catch-up)                │ │
│  │ • Audio commands queued                                                  │ │
│  │ • Input snapshot frozen                                                  │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                               │
│  Phase 2: CARTRIDGE draw(alpha)                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │ • gfx3d.* calls record to 3D command buffer (scene state)               │ │
│  │ • gfx.* calls record to 2D command buffer                               │ │
│  │ • gfx3d.render() marks 3D scene ready for rendering                     │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                               │
│  Phase 3: 3D RENDER (First - renders to CBUF)                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │ • Execute 3D scene render pass into CBUF color + depth                  │ │
│  │ • PBR lighting, shadows, environment mapping                            │ │
│  │ • Post-processing (tonemapping, bloom, SSAO if enabled)                 │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                              ↓                                                │
│  Phase 4: 2D CANVAS RENDER (Second - composites over CBUF)                    │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │ • Execute ThorVG command buffer to CPU surface                          │ │
│  │ • Upload CPU surface to GPU texture                                     │ │
│  │ • Composite 2D texture over CBUF with premultiplied alpha               │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                              ↓                                                │
│  Phase 5: PRESENT (Third - scales CBUF to backbuffer)                         │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │ • Apply present mode (fit/integer_nearest/fill/stretch)                 │ │
│  │ • Sample CBUF texture with appropriate filter (nearest/linear)          │ │
│  │ • Render to backbuffer with letterbox                                   │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                              ↓                                                │
│  Phase 6: WORKBENCH UI (Fourth - overlays on backbuffer)                      │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │ • Render ImGui in display space                                         │ │
│  │ • Editor, console, diagnostics, profiler                                │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                              ↓                                                │
│  Phase 7: SWAPCHAIN PRESENT                                                   │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │ • Present backbuffer to display                                         │ │
│  │ • VSync if enabled                                                      │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 7B.2.2 Key Ordering Guarantees (Normative)

1. **3D ALWAYS renders BEFORE 2D**: The 3D scene is the background; 2D is overlaid.
2. **2D composites with alpha**: 2D elements with alpha < 1 show the 3D scene beneath.
3. **Cartridges cannot change this order**: The rendering pipeline is fixed.
4. **gfx3d.render() is required**: If cartridge doesn't call `gfx3d.render()`, no 3D content appears.

### 7B.2.3 Render Order Diagram

```
CBUF Initial State: [undefined]
         │
         ▼
┌─────────────────────────┐
│   3D Render Pass        │  ← gfx3d.render() result
│   - Clear to sky color  │
│   - Draw all meshes     │
│   - Post-processing     │
└─────────────────────────┘
         │
         ▼
CBUF State: [3D scene rendered]
         │
         ▼
┌─────────────────────────┐
│   2D Canvas Composite   │  ← All gfx.* commands
│   - Blend over CBUF     │
│   - Premultiplied alpha │
│   - Vector graphics     │
│   - UI elements         │
└─────────────────────────┘
         │
         ▼
CBUF State: [3D + 2D overlay]
         │
         ▼
┌─────────────────────────┐
│   Present Pass          │
│   - Scale to backbuffer │
│   - Apply present mode  │
└─────────────────────────┘
         │
         ▼
Backbuffer: [Scaled cartridge output]
         │
         ▼
┌─────────────────────────┐
│   Workbench UI          │
│   - ImGui overlay       │
└─────────────────────────┘
         │
         ▼
Display: [Final output]
```

---

## 7B.3 Retained Mode 3D Rendering Model (Normative)

### 7B.3.1 Scene State Model

Unlike immediate-mode rendering, ARCANEE 3D uses a **retained scene graph**:

1. **Scene State**: The `gfx3d.*` calls modify a persistent scene data structure.
2. **No Command Buffer**: Changes to entities, transforms, materials are applied immediately to the scene state.
3. **Render Snapshot**: When `gfx3d.render()` is called, the current scene state is rendered.

```cpp
// Scene state is PERSISTENT between frames
class Scene3D {
public:
    // Entity management (persistent)
    EntityId createEntity(const std::string& name);
    void destroyEntity(EntityId e);
    
    // Transform modification (applied immediately to scene state)
    void setTransform(EntityId e, const Transform& t);
    
    // Mesh renderer (applied immediately)
    void addMeshRenderer(EntityId e, MeshHandle mesh, MaterialHandle mat);
    
    // When render() is called, current state is rendered
    void render(RenderContext& ctx);
    
private:
    std::unordered_map<EntityId, Entity> m_entities;
    std::vector<MeshRenderer> m_meshRenderers;
    std::vector<Light> m_lights;
    Camera m_activeCamera;
};
```

### 7B.3.2 Frame Lifecycle for 3D

```
Frame N-1:
  Scene state: [entities A, B, C at positions P1, P2, P3]
  
Frame N draw():
  gfx3d.setTransform(A, newPos)  → Scene state updated immediately
  gfx3d.setTransform(B, newPos)  → Scene state updated immediately  
  gfx3d.destroyEntity(C)         → Entity removed from scene
  gfx3d.render()                 → Current state rendered to CBUF

Frame N+1 draw():
  Scene state: [entities A, B at updated positions]
  (C is gone, no additional commands needed)
```

### 7B.3.3 Comparison: 2D vs 3D Rendering Models

| Aspect | 2D Canvas (gfx.*) | 3D Scene (gfx3d.*) |
|--------|-------------------|---------------------|
| Model | Immediate/Command Buffer | Retained Scene Graph |
| State persistence | Cleared each frame | Persistent until modified |
| When to render | Execute command buffer | Call gfx3d.render() |
| Typical usage | Draw calls each frame | Modify only what changes |

---

## 7B.4 DiligentEngine Integration (Normative)

### 7B.4.1 Device and Context Setup

```cpp
class Renderer3D {
public:
    void initialize(Diligent::IRenderDevice* device,
                    Diligent::IDeviceContext* context,
                    Diligent::ISwapChain* swapchain);
    
    void setCBUF(Diligent::ITextureView* cbufRTV,
                  Diligent::ITextureView* cbufDSV,
                  int width, int height);
    
    void renderScene(Scene3D* scene);
    
private:
    Diligent::IRenderDevice* m_device;
    Diligent::IDeviceContext* m_context;
    
    // CBUF targets
    Diligent::ITextureView* m_cbufRTV;
    Diligent::ITextureView* m_cbufDSV;
    
    // Pipeline states
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pbrPSO;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_skyboxPSO;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_tonemapPSO;
    
    // Constant buffers
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_frameConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_objectConstants;
};
```

### 7B.4.2 Render Target Setup (Normative)

CBUF is created as a DiligentEngine texture:

```cpp
void createCBUF(int width, int height) {
    Diligent::TextureDesc texDesc;
    texDesc.Name = "CBUF Color";
    texDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.Format = Diligent::TEX_FORMAT_BGRA8_UNORM;  // Preferred
    texDesc.BindFlags = Diligent::BIND_RENDER_TARGET | 
                        Diligent::BIND_SHADER_RESOURCE;
    texDesc.Usage = Diligent::USAGE_DEFAULT;
    
    m_device->CreateTexture(texDesc, nullptr, &m_cbufColor);
    m_cbufColor->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET, &m_cbufRTV);
    m_cbufColor->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE, &m_cbufSRV);
    
    // Depth buffer
    texDesc.Name = "CBUF Depth";
    texDesc.Format = Diligent::TEX_FORMAT_D32_FLOAT;
    texDesc.BindFlags = Diligent::BIND_DEPTH_STENCIL;
    
    m_device->CreateTexture(texDesc, nullptr, &m_cbufDepth);
    m_cbufDepth->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL, &m_cbufDSV);
}
```

### 7B.4.3 PBR Pipeline Architecture

```cpp
// Vertex shader input
struct VSInput {
    float3 position;
    float3 normal;
    float2 texcoord;
    float4 tangent;  // w = handedness
};

// Frame constants (once per frame)
struct FrameConstants {
    float4x4 viewProj;
    float4x4 view;
    float4x4 invView;
    float3 cameraPos;
    float time;
    float3 ambientColor;
    float pad;
};

// Object constants (per draw call)
struct ObjectConstants {
    float4x4 world;
    float4x4 normalMatrix;
};

// Material constants (per material)
struct MaterialConstants {
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float3 emissiveFactor;
    float alphaCutoff;
    uint alphaMode;  // 0=opaque, 1=mask, 2=blend
    uint doubleSided;
    uint pad[2];
};
```

### 7B.4.4 Scene Render Pass (Normative)

```cpp
void Renderer3D::renderScene(Scene3D* scene) {
    // 1. Set render targets to CBUF
    m_context->SetRenderTargets(1, &m_cbufRTV, m_cbufDSV,
                                 Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // 2. Clear CBUF
    float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};  // Or sky color
    m_context->ClearRenderTarget(m_cbufRTV, clearColor,
                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_context->ClearDepthStencil(m_cbufDSV, 
                                  Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0,
                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // 3. Update frame constants
    Camera& cam = scene->getActiveCamera();
    FrameConstants fc;
    fc.viewProj = cam.getViewProjectionMatrix();
    fc.view = cam.getViewMatrix();
    fc.invView = glm::inverse(cam.getViewMatrix());
    fc.cameraPos = cam.getPosition();
    fc.time = m_runtime->getTimeSeconds();
    fc.ambientColor = scene->getAmbientColor();
    updateBuffer(m_frameConstants, &fc, sizeof(fc));
    
    // 4. Render opaque objects
    m_context->SetPipelineState(m_pbrPSO);
    for (const auto& renderer : scene->getMeshRenderers()) {
        if (!renderer.visible) continue;
        if (renderer.material->alphaMode == AlphaMode::Blend) continue;  // Deferred
        
        drawMeshRenderer(renderer);
    }
    
    // 5. Render transparent objects (back-to-front sorted)
    std::vector<MeshRenderer*> transparentObjects;
    for (auto& renderer : scene->getMeshRenderers()) {
        if (renderer.material->alphaMode == AlphaMode::Blend) {
            transparentObjects.push_back(&renderer);
        }
    }
    sortBackToFront(transparentObjects, cam.getPosition());
    
    for (const auto* renderer : transparentObjects) {
        drawMeshRenderer(*renderer);
    }
    
    // 6. Post-processing (optional)
    if (m_postProcessEnabled) {
        applyTonemap();
        if (m_bloomEnabled) applyBloom();
        if (m_ssaoEnabled) applySSAO();
    }
}
```

---

## 7B.5 Scene Graph Data Structures (Normative)

### 7B.5.1 Entity System

```cpp
using EntityId = uint32_t;

struct Transform {
    glm::vec3 position = {0, 0, 0};
    glm::quat rotation = glm::identity<glm::quat>();
    glm::vec3 scale = {1, 1, 1};
    
    glm::mat4 getMatrix() const {
        return glm::translate(glm::mat4(1.0f), position)
             * glm::mat4_cast(rotation)
             * glm::scale(glm::mat4(1.0f), scale);
    }
};

struct Entity {
    EntityId id;
    std::string name;
    Transform transform;
    
    // Components (nullable)
    MeshRenderer* meshRenderer = nullptr;
    Light* light = nullptr;
    Camera* camera = nullptr;
};

class EntityManager {
public:
    EntityId createEntity(const std::string& name = "");
    void destroyEntity(EntityId e);
    
    Entity* getEntity(EntityId e);
    
    void setTransform(EntityId e, const Transform& t);
    Transform getTransform(EntityId e) const;
    
private:
    HandlePool<Entity> m_entities;
    EntityId m_nextId = 1;
};
```

### 7B.5.2 Mesh and Material Handles

```cpp
using MeshHandle = uint32_t;
using MaterialHandle = uint32_t;
using TextureHandle = uint32_t;

struct Mesh {
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
    uint32_t indexCount;
    uint32_t vertexCount;
    BoundingBox bounds;
};

struct Material {
    glm::vec4 baseColorFactor = {1, 1, 1, 1};
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    glm::vec3 emissiveFactor = {0, 0, 0};
    
    TextureHandle baseColorTex = 0;
    TextureHandle metallicRoughnessTex = 0;
    TextureHandle normalTex = 0;
    TextureHandle emissiveTex = 0;
    TextureHandle occlusionTex = 0;
    
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};

struct MeshRenderer {
    EntityId entity;
    MeshHandle mesh;
    MaterialHandle material;
    bool visible = true;
};
```

### 7B.5.3 Lights

```cpp
enum class LightType { Directional, Point, Spot };

struct Light {
    LightType type;
    EntityId entity;  // Position/direction from entity transform
    
    glm::vec3 color = {1, 1, 1};
    float intensity = 1.0f;
    
    // Point/Spot only
    float range = 10.0f;
    
    // Spot only
    float innerAngle = glm::radians(30.0f);
    float outerAngle = glm::radians(45.0f);
};

class LightManager {
public:
    static constexpr int MAX_LIGHTS = 256;
    
    LightHandle createLight(LightType type);
    void destroyLight(LightHandle h);
    void setLightParams(LightHandle h, const LightParams& params);
    
    // For shader: get lights affecting a point
    void getLightsForRendering(std::vector<LightData>& outLights);
    
private:
    HandlePool<Light> m_lights;
};
```

### 7B.5.4 Camera

```cpp
struct Camera {
    CameraHandle handle;
    EntityId entity = 0;  // Optional attachment
    
    bool isPerspective = true;
    
    // Perspective params
    float fovY = glm::radians(60.0f);
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    
    // Orthographic params
    float orthoHeight = 10.0f;
    
    // Manual view (if not attached to entity)
    glm::vec3 eye = {0, 0, 5};
    glm::vec3 at = {0, 0, 0};
    glm::vec3 up = {0, 1, 0};
    
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;
};
```

---

## 7B.6 glTF Loading Integration (Normative)

### 7B.6.1 glTF Loader Architecture

```cpp
struct GLTFLoadResult {
    SceneHandle scene;
    EntityId rootEntity;
    std::vector<MeshHandle> meshes;
    std::vector<MaterialHandle> materials;
    std::vector<TextureHandle> textures;
    std::vector<AnimHandle> animations;  // v0.1: may be empty
};

class GLTFLoader {
public:
    GLTFLoader(ResourceManager* resources, VFS* vfs);
    
    GLTFLoadResult load(const std::string& vfsPath, const GLTFLoadOptions& opts);
    
private:
    // Parsing (using cgltf or tinygltf)
    bool parseGLTF(const std::vector<uint8_t>& data, cgltf_data** outData);
    
    // Resource creation
    MeshHandle createMesh(const cgltf_mesh* mesh, const cgltf_data* data);
    MaterialHandle createMaterial(const cgltf_material* mat);
    TextureHandle createTexture(const cgltf_image* img, bool sRGB);
    
    // Entity hierarchy
    EntityId createEntityHierarchy(const cgltf_node* node, EntityId parent);
    
private:
    ResourceManager* m_resources;
    VFS* m_vfs;
    std::string m_basePath;  // For relative file references
};
```

### 7B.6.2 Deterministic Loading Order (Normative)

glTF loading MUST produce deterministic results:

```cpp
GLTFLoadResult GLTFLoader::load(const std::string& vfsPath, 
                                 const GLTFLoadOptions& opts) {
    GLTFLoadResult result;
    
    // 1. Read file
    std::vector<uint8_t> data;
    if (!m_vfs->readBytes(vfsPath, data)) {
        setLastError("gfx3d.loadGLTF: file not found: " + vfsPath);
        return result;
    }
    
    m_basePath = getDirectoryPath(vfsPath);  // For relative references
    
    // 2. Parse
    cgltf_data* gltf = nullptr;
    if (!parseGLTF(data, &gltf)) {
        return result;
    }
    
    // 3. Load textures first (deterministic order: by index)
    for (size_t i = 0; i < gltf->images_count; ++i) {
        bool sRGB = isColorTexture(gltf->images[i]);  // Determine from usage
        TextureHandle h = createTexture(&gltf->images[i], sRGB);
        result.textures.push_back(h);
    }
    
    // 4. Load materials (deterministic order: by index)
    for (size_t i = 0; i < gltf->materials_count; ++i) {
        MaterialHandle h = createMaterial(&gltf->materials[i]);
        result.materials.push_back(h);
    }
    
    // 5. Load meshes (deterministic order: by index)
    for (size_t i = 0; i < gltf->meshes_count; ++i) {
        MeshHandle h = createMesh(&gltf->meshes[i], gltf);
        result.meshes.push_back(h);
    }
    
    // 6. Create scene and entity hierarchy
    result.scene = m_resources->createScene();
    m_resources->setActiveScene(result.scene);
    
    // Process default scene or first scene
    const cgltf_scene* scene = gltf->scene ? gltf->scene : &gltf->scenes[0];
    
    // Create root entity
    result.rootEntity = m_resources->createEntity("glTF_root");
    
    // 7. Create entities from nodes (deterministic depth-first order)
    for (size_t i = 0; i < scene->nodes_count; ++i) {
        createEntityHierarchy(scene->nodes[i], result.rootEntity);
    }
    
    cgltf_free(gltf);
    return result;
}
```

---

## 7B.7 API Command Recording (Normative)

While 3D uses a retained scene model, some commands are still recorded for execution:

### 7B.7.1 Render Command

```cpp
// gfx3d.render() records a render command
void Gfx3DBindings::render(HSQUIRRELVM vm) {
    // Validate active scene and camera
    Scene3D* scene = m_runtime->getActiveScene();
    if (!scene) {
        setLastError("gfx3d.render: no active scene");
        sq_pushbool(vm, SQFalse);
        return;
    }
    
    Camera* cam = scene->getActiveCamera();
    if (!cam) {
        setLastError("gfx3d.render: no active camera");
        sq_pushbool(vm, SQFalse);
        return;
    }
    
    // Record render request for execution phase
    m_renderQueue->enqueue3DRender(scene);
    
    sq_pushbool(vm, SQTrue);
}
```

### 7B.7.2 Render Queue Execution

```cpp
void RenderPipeline::executeFrame() {
    // Phase 3: Execute 3D renders
    while (Render3DRequest* req = m_renderQueue->pop3DRequest()) {
        m_renderer3d->renderScene(req->scene);
    }
    
    // Phase 4: Execute 2D canvas
    m_canvas2d->executeCommandBuffer();
    m_canvas2d->uploadToGPU();
    m_canvas2d->compositeOverCBUF();
    
    // Phase 5: Present CBUF to backbuffer
    m_presentPass->execute();
    
    // Phase 6: Workbench UI
    m_workbench->renderUI();
    
    // Phase 7: Present swapchain
    m_swapchain->Present(m_vsync ? 1 : 0);
}
```

---

## 7B.8 2D Over 3D Composition (Normative)

### 7B.8.1 Composition Pass

After 3D rendering, the 2D canvas is composited:

```cpp
void compose2DOverCBUF() {
    // Transition CBUF to shader resource
    m_context->TransitionResource(m_cbufColor, 
        Diligent::RESOURCE_STATE_SHADER_RESOURCE);
    
    // Set composition render target (CBUF)
    m_context->SetRenderTargets(1, &m_cbufRTV, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Draw 2D canvas texture using premultiplied alpha blend
    m_context->SetPipelineState(m_composePSO);
    
    // Bind 2D canvas texture
    m_context->SetTexture(0, m_canvas2dTextureSRV);
    
    // Draw full-screen quad
    m_context->Draw({.NumVertices = 3});  // Full-screen triangle
}
```

### 7B.8.2 Blend State for Composition (Normative)

```cpp
Diligent::BlendStateDesc createCompositeBlendState() {
    Diligent::BlendStateDesc bs;
    bs.RenderTargets[0].BlendEnable = true;
    
    // Premultiplied alpha blending
    bs.RenderTargets[0].SrcBlend = Diligent::BLEND_FACTOR_ONE;
    bs.RenderTargets[0].DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    bs.RenderTargets[0].BlendOp = Diligent::BLEND_OPERATION_ADD;
    
    bs.RenderTargets[0].SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
    bs.RenderTargets[0].DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    bs.RenderTargets[0].BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;
    
    return bs;
}
```

---

## 7B.9 Resource Lifetime Management (Normative)

### 7B.9.1 Handle Validation

```cpp
template<typename T>
class HandlePool {
public:
    Handle allocate() {
        if (m_freeList.empty()) {
            Handle h = m_nextHandle++;
            m_data.emplace(h, T{});
            return h;
        }
        
        Handle h = m_freeList.back();
        m_freeList.pop_back();
        m_data.emplace(h, T{});
        return h;
    }
    
    void free(Handle h) {
        auto it = m_data.find(h);
        if (it != m_data.end()) {
            m_data.erase(it);
            m_freeList.push_back(h);
        }
    }
    
    bool valid(Handle h) const {
        return m_data.find(h) != m_data.end();
    }
    
    T* get(Handle h) {
        auto it = m_data.find(h);
        return (it != m_data.end()) ? &it->second : nullptr;
    }
    
private:
    std::unordered_map<Handle, T> m_data;
    std::vector<Handle> m_freeList;
    Handle m_nextHandle = 1;  // 0 is invalid
};
```

### 7B.9.2 Cartridge Resource Cleanup

```cpp
void ResourceManager::cleanupCartridgeResources(CartridgeId cartridge) {
    // Cleanup in reverse dependency order
    
    // 1. Destroy mesh renderers
    for (auto& [id, entity] : m_entities) {
        if (entity.owner == cartridge) {
            if (entity.meshRenderer) {
                destroyMeshRenderer(entity.meshRenderer);
            }
        }
    }
    
    // 2. Destroy entities
    std::vector<EntityId> toDestroy;
    for (auto& [id, entity] : m_entities) {
        if (entity.owner == cartridge) {
            toDestroy.push_back(id);
        }
    }
    for (EntityId id : toDestroy) {
        destroyEntity(id);
    }
    
    // 3. Destroy cameras and lights
    // ... similar pattern
    
    // 4. Destroy materials
    // ... 
    
    // 5. Destroy meshes
    // ...
    
    // 6. Destroy textures
    // ...
    
    // 7. Destroy scenes
    // ...
}
```

---

## 7B.10 Compliance Checklist (Chapter 7B)

An implementation is compliant with Chapter 7B if it:

1. Implements the **exact render order**: 3D → 2D Canvas → Present → UI.
2. Uses **retained scene graph** for 3D state management.
3. Renders 3D scene only when `gfx3d.render()` is called.
4. Composites 2D canvas **over** CBUF with premultiplied alpha blending.
5. Implements deterministic glTF loading with consistent entity/resource ordering.
6. Uses DiligentEngine for all GPU operations with proper resource state transitions.
7. Validates all handles and fails safely with appropriate error messages.
8. Cleans up all cartridge-owned 3D resources on stop/reload.

---

*End of Chapter 7B*

---
© 2025 Michele Fabbri. Licensed under AGPL-3.0.
