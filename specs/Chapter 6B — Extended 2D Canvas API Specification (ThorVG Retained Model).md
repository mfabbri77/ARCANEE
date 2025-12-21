# Chapter 6B — Extended 2D Canvas API Specification (ThorVG Retained Model)

## 6B.1 Purpose and Scope

This supplementary chapter provides **detailed implementation-level specifications** for the 2D Canvas API, extending Chapter 6 with:

* Complete ThorVG API mapping to ARCANEE `gfx.*` functions
* Retained mode rendering model (command recording and deferred execution)
* Internal architecture and data structures
* Precise behavioral semantics for edge cases
* Reference pseudocode for implementers

---

## 6B.2 Retained Mode Rendering Model (Normative)

### 6B.2.1 Core Concept

ARCANEE's 2D Canvas API uses a **retained mode** architecture rather than immediate mode. This means:

1. **Command Recording Phase**: During `draw(alpha)`, all `gfx.*` calls are recorded into an internal command buffer rather than executed immediately.
2. **Scene Synchronization**: At the end of `draw(alpha)`, the recorded commands represent a complete, self-contained "2D scene" for that frame.
3. **Render Execution Phase**: The runtime executes all recorded commands in order against ThorVG, producing the final rasterized output.

### 6B.2.2 Rationale

The retained mode approach provides:

* **Determinism**: The complete draw sequence is captured before any rendering occurs
* **Batching Opportunities**: Commands can be optimized/batched before submission to ThorVG
* **Debugging Support**: The command buffer can be inspected for profiling and debugging
* **Thread Safety**: Recording and execution can be decoupled if needed

### 6B.2.3 Command Buffer Lifecycle (Normative)

```
┌─────────────────────────────────────────────────────────────────┐
│                      Frame N Timeline                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  update(dt)     draw(alpha)           Render Phase               │
│  ────────────   ───────────────────   ────────────────────────   │
│                 │                  │  │                       │  │
│                 │ gfx.* calls      │  │ Execute CommandBuffer │  │
│                 │ record to        │  │ against ThorVG        │  │
│                 │ CommandBuffer    │  │                       │  │
│                 │                  │  │ Upload to GPU texture │  │
│                 └──────────────────┘  └───────────────────────┘  │
│                                                                  │
│  CommandBuffer is CLEARED at start of draw()                    │
│  CommandBuffer is EXECUTED after draw() returns                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6B.2.4 Command Buffer Structure (Normative)

```cpp
// NORMATIVE reference structure

enum class GfxCommand : uint16_t {
    // State Management
    Save,
    Restore,
    ResetTransform,
    SetTransform,
    Translate,
    Rotate,
    Scale,
    SetGlobalAlpha,
    SetBlend,
    
    // Style Commands
    SetFillColor,
    SetStrokeColor,
    SetLineWidth,
    SetLineJoin,
    SetLineCap,
    SetMiterLimit,
    SetLineDash,
    SetFillPaint,
    SetStrokePaint,
    
    // Path Commands
    BeginPath,
    ClosePath,
    MoveTo,
    LineTo,
    QuadTo,
    CubicTo,
    Arc,
    Rect,
    
    // Drawing Commands
    Fill,
    Stroke,
    Clip,
    ResetClip,
    FillRect,
    StrokeRect,
    ClearRect,
    Clear,
    
    // Image Commands
    DrawImage,
    DrawImageRect,
    
    // Text Commands
    SetFont,
    SetTextAlign,
    SetTextBaseline,
    FillText,
    StrokeText,
    
    // Surface Commands
    SetSurface,
};

struct CommandData {
    GfxCommand cmd;
    uint16_t flags;
    
    union {
        struct { float a, b, c, d, e, f; } transform;
        struct { float x, y; } point2;
        struct { float x, y, w, h; } rect;
        struct { float cx, cy, x, y; } quadratic;
        struct { float c1x, c1y, c2x, c2y, x, y; } cubic;
        struct { float x, y, r, a0, a1; bool ccw; } arc;
        struct { uint32_t color; } color;
        struct { float value; } scalar;
        struct { Handle handle; float dx, dy; } image;
        struct { Handle handle; int sx, sy, sw, sh; float dx, dy, dw, dh; } imageRect;
        struct { Handle handle; } handleOnly;
        struct { const char* text; float x, y, maxWidth; bool hasMaxWidth; } text;
        // ... additional variants as needed
    };
};

class Canvas2DCommandBuffer {
public:
    void clear();
    void record(const CommandData& cmd);
    
    // Iteration for execution
    size_t size() const;
    const CommandData& operator[](size_t i) const;
    
private:
    std::vector<CommandData> m_commands;
    BumpAllocator m_stringPool;  // For text data
};
```

---

## 6B.3 Complete ThorVG API Mapping (Normative)

This section maps every ARCANEE `gfx.*` function to its corresponding ThorVG operations.

### 6B.3.1 Canvas and Surface Management

| ARCANEE API | ThorVG Mapping | Notes |
|-------------|----------------|-------|
| `gfx.createSurface(w, h)` | `tvg::SwCanvas::mempool()` allocation | Creates new SwCanvas with dedicated buffer |
| `gfx.freeSurface(s)` | Destroy SwCanvas, free buffer | Invalidates handle |
| `gfx.setSurface(s)` | Switch active SwCanvas for command execution | Commands target this surface |
| `gfx.clear(color)` | `canvas->clear(true)` + colored rect fill | ThorVG clear + colored background |
| `gfx.getTargetSize()` | Query current SwCanvas dimensions | Return (width, height) |

### 6B.3.2 Transform Operations

| ARCANEE API | ThorVG Mapping | Notes |
|-------------|----------------|-------|
| `gfx.save()` | Push current matrix to stack | Stack is managed by ARCANEE |
| `gfx.restore()` | Pop matrix from stack | Must match save() |
| `gfx.resetTransform()` | Set current matrix to identity | `Matrix{1,0,0,0,1,0,0,0,1}` |
| `gfx.setTransform(a,b,c,d,e,f)` | Set matrix directly | Canvas-style matrix |
| `gfx.translate(x, y)` | Post-multiply translation | `matrix = matrix * T(x,y)` |
| `gfx.rotate(rad)` | Post-multiply rotation | `matrix = matrix * R(rad)` |
| `gfx.scale(sx, sy)` | Post-multiply scale | `matrix = matrix * S(sx,sy)` |

**Matrix Format Mapping (Normative)**:

ARCANEE uses Canvas-style 2D affine matrix `(a, b, c, d, e, f)`:
```
| a  c  e |
| b  d  f |
| 0  0  1 |
```

ThorVG uses `tvg::Matrix`:
```cpp
struct Matrix {
    float e11, e12, e13;  // Row 1
    float e21, e22, e23;  // Row 2
    float e31, e32, e33;  // Row 3 (0, 0, 1 for 2D)
};
```

**Conversion (Normative)**:
```cpp
tvg::Matrix toThorVG(float a, float b, float c, float d, float e, float f) {
    return {
        a,   c,   e,    // e11, e12, e13
        b,   d,   f,    // e21, e22, e23
        0.f, 0.f, 1.f   // e31, e32, e33
    };
}
```

### 6B.3.3 Path Construction

| ARCANEE API | ThorVG Mapping | Notes |
|-------------|----------------|-------|
| `gfx.beginPath()` | Create new `tvg::Shape` | Store as "current path shape" |
| `gfx.closePath()` | `shape->close()` | Closes current subpath |
| `gfx.moveTo(x, y)` | `shape->moveTo(x, y)` | Start new subpath |
| `gfx.lineTo(x, y)` | `shape->lineTo(x, y)` | Append line segment |
| `gfx.quadTo(cx, cy, x, y)` | Convert to cubic | ThorVG lacks quadratic; use cubic approximation |
| `gfx.cubicTo(c1x, c1y, c2x, c2y, x, y)` | `shape->cubicTo(...)` | Cubic Bézier |
| `gfx.arc(x, y, r, a0, a1, ccw)` | `shape->appendArc(...)` | See §6B.3.4 |
| `gfx.rect(x, y, w, h)` | `shape->appendRect(x, y, w, h, 0, 0)` | Rectangle path |

### 6B.3.4 Arc Implementation (Normative)

ThorVG's `appendArc` differs from Canvas arc. ARCANEE MUST implement arc conversion:

```cpp
// NORMATIVE arc implementation
void appendArcToShape(tvg::Shape* shape, float cx, float cy, float r, 
                       float startAngle, float endAngle, bool counterclockwise) {
    // Calculate sweep
    float sweep = endAngle - startAngle;
    
    // Adjust for counterclockwise
    if (counterclockwise) {
        if (sweep > 0) sweep -= 2 * M_PI;
    } else {
        if (sweep < 0) sweep += 2 * M_PI;
    }
    
    // Convert to degrees for ThorVG
    float startDeg = startAngle * (180.0f / M_PI);
    float sweepDeg = sweep * (180.0f / M_PI);
    
    // ThorVG appendArc: center-based arc
    // Parameters: cx, cy, rx, ry, startAngle, sweep, pie
    shape->appendArc(cx, cy, r, r, startDeg, sweepDeg, false);
}
```

### 6B.3.5 Quadratic to Cubic Conversion (Normative)

ThorVG does not have native quadratic curves. ARCANEE MUST convert:

```cpp
// NORMATIVE quadratic to cubic conversion
void quadraticToCubic(float x0, float y0,   // Start point (current point)
                      float cx, float cy,    // Control point
                      float x, float y,      // End point
                      float& c1x, float& c1y,
                      float& c2x, float& c2y) {
    // Q(t) = P0*(1-t)^2 + 2*P1*(1-t)*t + P2*t^2
    // C(t) = P0*(1-t)^3 + 3*C1*(1-t)^2*t + 3*C2*(1-t)*t^2 + P3*t^3
    // 
    // C1 = P0 + 2/3*(P1 - P0)
    // C2 = P2 + 2/3*(P1 - P2)
    
    c1x = x0 + (2.0f / 3.0f) * (cx - x0);
    c1y = y0 + (2.0f / 3.0f) * (cy - y0);
    c2x = x + (2.0f / 3.0f) * (cx - x);
    c2y = y + (2.0f / 3.0f) * (cy - y);
}
```

### 6B.3.6 Fill and Stroke Operations

| ARCANEE API | ThorVG Mapping | Notes |
|-------------|----------------|-------|
| `gfx.fill()` | Shape with fill style, push to canvas | Clone shape if needed for later stroke |
| `gfx.stroke()` | Shape with stroke style, push to canvas | Apply current stroke parameters |
| `gfx.setFillColor(color)` | Store for `shape->fill(r, g, b, a)` | Extract ARGB components |
| `gfx.setStrokeColor(color)` | Store for `shape->stroke(r, g, b, a)` | Extract ARGB components |
| `gfx.setLineWidth(w)` | `shape->stroke(w)` | Stroke width in pixels |
| `gfx.setLineJoin(join)` | `shape->stroke(StrokeJoin::*)` | Miter/Round/Bevel |
| `gfx.setLineCap(cap)` | `shape->stroke(StrokeCap::*)` | Butt/Round/Square |
| `gfx.setMiterLimit(v)` | `shape->stroke(..., v)` | Miter limit value |
| `gfx.setLineDash(dashes, offset)` | `shape->stroke(dashes.data(), count)` | Dash pattern array |

**Color Extraction (Normative)**:
```cpp
void extractARGB(uint32_t color, uint8_t& a, uint8_t& r, uint8_t& g, uint8_t& b) {
    a = (color >> 24) & 0xFF;
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
}
```

**ThorVG Enum Mappings (Normative)**:

```cpp
tvg::StrokeJoin mapLineJoin(const std::string& join) {
    if (join == "miter") return tvg::StrokeJoin::Miter;
    if (join == "round") return tvg::StrokeJoin::Round;
    if (join == "bevel") return tvg::StrokeJoin::Bevel;
    return tvg::StrokeJoin::Miter;  // Default
}

tvg::StrokeCap mapLineCap(const std::string& cap) {
    if (cap == "butt")   return tvg::StrokeCap::Butt;
    if (cap == "round")  return tvg::StrokeCap::Round;
    if (cap == "square") return tvg::StrokeCap::Square;
    return tvg::StrokeCap::Butt;  // Default
}
```

### 6B.3.7 Blend Modes

| ARCANEE Blend | ThorVG CompositeMethod | Notes |
|---------------|------------------------|-------|
| `"normal"` | `BlendMethod::Normal` | Default source-over |
| `"multiply"` | `BlendMethod::Multiply` | Multiply blend |
| `"screen"` | `BlendMethod::Screen` | Screen blend |
| `"overlay"` | `BlendMethod::Overlay` | Overlay blend |
| `"darken"` | `BlendMethod::Darken` | Darken blend |
| `"lighten"` | `BlendMethod::Lighten` | Lighten blend |
| `"colorDodge"` | `BlendMethod::ColorDodge` | Color dodge |
| `"colorBurn"` | `BlendMethod::ColorBurn` | Color burn |
| `"hardLight"` | `BlendMethod::HardLight` | Hard light |
| `"softLight"` | `BlendMethod::SoftLight` | Soft light |
| `"difference"` | `BlendMethod::Difference` | Difference |
| `"exclusion"` | `BlendMethod::Exclusion` | Exclusion |
| `"add"` | `BlendMethod::Add` | Additive |

**Blend Mode Setting (Normative)**:

```cpp
bool setBlendMode(tvg::Shape* shape, const std::string& mode) {
    static const std::unordered_map<std::string, tvg::BlendMethod> blendMap = {
        {"normal",     tvg::BlendMethod::Normal},
        {"multiply",   tvg::BlendMethod::Multiply},
        {"screen",     tvg::BlendMethod::Screen},
        {"overlay",    tvg::BlendMethod::Overlay},
        {"darken",     tvg::BlendMethod::Darken},
        {"lighten",    tvg::BlendMethod::Lighten},
        {"colorDodge", tvg::BlendMethod::ColorDodge},
        {"colorBurn",  tvg::BlendMethod::ColorBurn},
        {"hardLight",  tvg::BlendMethod::HardLight},
        {"softLight",  tvg::BlendMethod::SoftLight},
        {"difference", tvg::BlendMethod::Difference},
        {"exclusion",  tvg::BlendMethod::Exclusion},
        {"add",        tvg::BlendMethod::Add},
    };
    
    auto it = blendMap.find(mode);
    if (it == blendMap.end()) return false;
    
    shape->blend(it->second);
    return true;
}
```

### 6B.3.8 Clipping

| ARCANEE API | ThorVG Mapping | Notes |
|-------------|----------------|-------|
| `gfx.clip()` | Create ClipPath from current path | Push to clip stack |
| `gfx.resetClip()` | Clear clip for current state level | Does not affect saved states |

**Clipping Implementation (Normative)**:

```cpp
// Clip stack management
struct CanvasState {
    tvg::Matrix transform;
    float globalAlpha;
    tvg::BlendMethod blendMode;
    // ... other state
    
    std::unique_ptr<tvg::Shape> clipShape;  // Current clip path (transformed)
};

void Canvas2D::applyClip(tvg::Shape* drawShape, const CanvasState& state) {
    if (state.clipShape) {
        // Clone clip and set as composite
        auto clipClone = state.clipShape->duplicate();
        drawShape->composite(std::move(clipClone), tvg::CompositeMethod::ClipPath);
    }
}
```

### 6B.3.9 Gradients

| ARCANEE API | ThorVG Mapping | Notes |
|-------------|----------------|-------|
| `gfx.createLinearGradient(x1, y1, x2, y2)` | `tvg::LinearGradient::gen()` | Set start/end points |
| `gfx.createRadialGradient(cx, cy, r)` | `tvg::RadialGradient::gen()` | Set center and radius |
| `gfx.paintSetStops(p, stops)` | `gradient->colorStops(...)` | Convert stop format |
| `gfx.paintSetSpread(p, mode)` | `gradient->spread(...)` | Pad/Repeat/Reflect |
| `gfx.setFillPaint(p)` | `shape->fill(std::move(gradient))` | Apply gradient as fill |
| `gfx.setStrokePaint(p)` | `shape->stroke(std::move(gradient))` | Apply gradient as stroke |

**Gradient Stop Conversion (Normative)**:

```cpp
// Convert ARCANEE stops to ThorVG colorStops
// ARCANEE: [ { offset: 0.0, color: 0xFFFF0000 }, { offset: 1.0, color: 0xFF0000FF } ]
// ThorVG: array of tvg::Fill::ColorStop

void setGradientStops(tvg::Fill* gradient, const std::vector<GradientStop>& stops) {
    std::vector<tvg::Fill::ColorStop> tvgStops;
    tvgStops.reserve(stops.size());
    
    for (const auto& s : stops) {
        tvg::Fill::ColorStop cs;
        cs.offset = s.offset;
        cs.r = (s.color >> 16) & 0xFF;
        cs.g = (s.color >> 8) & 0xFF;
        cs.b = s.color & 0xFF;
        cs.a = (s.color >> 24) & 0xFF;
        tvgStops.push_back(cs);
    }
    
    gradient->colorStops(tvgStops.data(), tvgStops.size());
}

tvg::FillSpread mapSpreadMode(const std::string& mode) {
    if (mode == "pad")     return tvg::FillSpread::Pad;
    if (mode == "repeat")  return tvg::FillSpread::Repeat;
    if (mode == "reflect") return tvg::FillSpread::Reflect;
    return tvg::FillSpread::Pad;  // Default
}
```

### 6B.3.10 Image Drawing

| ARCANEE API | ThorVG Mapping | Notes |
|-------------|----------------|-------|
| `gfx.loadImage(path)` | `tvg::Picture::load(buffer, size, ...)` | Load from VFS buffer |
| `gfx.drawImage(img, dx, dy)` | Position Picture at (dx, dy) | Apply current transform |
| `gfx.drawImageRect(img, sx, sy, sw, sh, dx, dy, dw, dh)` | Viewport + scale Picture | More complex; see below |

**Image Drawing Implementation (Normative)**:

```cpp
void Canvas2D::executeDrawImage(Handle imgHandle, float dx, float dy) {
    Image* img = m_resources->getImage(imgHandle);
    if (!img) return;
    
    // Clone the picture for this draw call
    auto picture = img->picture->duplicate();
    
    // Apply current transform + translation to (dx, dy)
    tvg::Matrix m = m_currentState.transform;
    // Translate matrix by (dx, dy) in local space
    m.e13 += dx * m.e11 + dy * m.e12;
    m.e23 += dx * m.e21 + dy * m.e22;
    
    picture->transform(m);
    picture->opacity(static_cast<uint8_t>(m_currentState.globalAlpha * 255));
    picture->blend(m_currentState.blendMode);
    
    applyClip(picture.get(), m_currentState);
    
    m_canvas->push(std::move(picture));
}

void Canvas2D::executeDrawImageRect(Handle imgHandle,
                                     int sx, int sy, int sw, int sh,
                                     float dx, float dy, float dw, float dh) {
    Image* img = m_resources->getImage(imgHandle);
    if (!img) return;
    
    auto picture = img->picture->duplicate();
    
    // Set viewport for source rect portion
    picture->viewbox(sx, sy, sw, sh);
    
    // Scale to destination size and position
    float scaleX = dw / sw;
    float scaleY = dh / sh;
    
    tvg::Matrix local = {
        scaleX, 0,      dx,
        0,      scaleY, dy,
        0,      0,      1
    };
    
    // Compose with current transform
    tvg::Matrix final = multiplyMatrices(m_currentState.transform, local);
    picture->transform(final);
    
    picture->opacity(static_cast<uint8_t>(m_currentState.globalAlpha * 255));
    picture->blend(m_currentState.blendMode);
    
    applyClip(picture.get(), m_currentState);
    
    m_canvas->push(std::move(picture));
}
```

### 6B.3.11 Text Rendering (ThorVG Native)

ThorVG 0.13+ provides native text rendering via `tvg::Text`. ARCANEE uses this directly without external font dependencies.

| ARCANEE API | ThorVG Mapping | Notes |
|-------------|----------------|-------|
| `gfx.loadFont(path, sizePx, opts)` | `tvg::Text::font()` registration | Register font family from file |
| `gfx.setFont(font)` | Store current font handle | State stack member |
| `gfx.setTextAlign(align)` | Store for positioning | Align applied at draw time |
| `gfx.setTextBaseline(base)` | Store for baseline offset | Offset applied at draw time |
| `gfx.measureText(text)` | ThorVG text bounds query | Return width, height, etc. |
| `gfx.fillText(text, x, y, maxWidth)` | `tvg::Text` with fill | Push to canvas |
| `gfx.strokeText(text, x, y, maxWidth)` | `tvg::Text` with stroke | Push to canvas |

---

## 6B.4 ThorVG Native Text Subsystem (Normative)

### 6B.4.1 ThorVG Text Architecture

ThorVG provides built-in text rendering. ARCANEE wraps this functionality:

```cpp
// NORMATIVE ThorVG native text implementation

class FontManager {
public:
    FontHandle loadFont(const std::string& vfsPath, int sizePx, const FontOptions& opts);
    void freeFont(FontHandle handle);
    
    TextMetrics measureText(FontHandle font, const std::string& text);
    
    // Render text using ThorVG native Text class
    std::unique_ptr<tvg::Text> createTextFill(FontHandle font, const std::string& text,
                                               float x, float y, TextAlign align, 
                                               TextBaseline baseline, uint32_t color);
    
    std::unique_ptr<tvg::Text> createTextStroke(FontHandle font, const std::string& text,
                                                  float x, float y, TextAlign align,
                                                  TextBaseline baseline, 
                                                  const StrokeParams& stroke);
    
private:
    struct LoadedFont {
        std::string familyName;   // Registered family name in ThorVG
        int sizePx;               // Nominal size
        std::vector<uint8_t> data; // Font file data kept in memory
    };
    
    HandlePool<LoadedFont> m_fonts;
    
    // Generate unique family name for each loaded font instance
    std::string generateFamilyName(FontHandle h) {
        return "arcanee_font_" + std::to_string(h);
    }
};
```

### 6B.4.2 Font Loading (Normative)

```cpp
FontHandle FontManager::loadFont(const std::string& vfsPath, int sizePx, 
                                  const FontOptions& opts) {
    // 1. Read font file from VFS
    std::vector<uint8_t> fontData;
    if (!m_vfs->readBytes(vfsPath, fontData)) {
        setLastError("gfx.loadFont: file not found: " + vfsPath);
        return 0;
    }
    
    // 2. Allocate handle
    FontHandle h = m_fonts.allocate();
    if (h == 0) {
        setLastError("gfx.loadFont: max fonts exceeded");
        return 0;
    }
    
    // 3. Register font with ThorVG using unique family name
    LoadedFont& font = m_fonts.get(h);
    font.familyName = generateFamilyName(h);
    font.sizePx = sizePx;
    font.data = std::move(fontData);
    
    // ThorVG font registration
    auto result = tvg::Text::load(font.data.data(), font.data.size(), 
                                   font.familyName.c_str(), "ttf");
    if (result != tvg::Result::Success) {
        m_fonts.free(h);
        setLastError("gfx.loadFont: ThorVG failed to load font");
        return 0;
    }
    
    return h;
}

void FontManager::freeFont(FontHandle handle) {
    if (!m_fonts.valid(handle)) return;
    
    LoadedFont& font = m_fonts.get(handle);
    
    // Unload from ThorVG (if supported by API)
    tvg::Text::unload(font.familyName.c_str());
    
    m_fonts.free(handle);
}
```

### 6B.4.3 Text Metrics (Normative)

```cpp
struct TextMetrics {
    float width;       // Advance width of the text
    float height;      // Total height (ascent + descent)
    float ascent;      // Distance from baseline to top
    float descent;     // Distance from baseline to bottom (positive)
    float lineHeight;  // Recommended line spacing
};

TextMetrics FontManager::measureText(FontHandle handle, const std::string& text) {
    if (!m_fonts.valid(handle)) return {};
    
    LoadedFont& font = m_fonts.get(handle);
    TextMetrics m = {};
    
    // Create temporary Text object to measure
    auto textObj = tvg::Text::gen();
    textObj->font(font.familyName.c_str(), static_cast<float>(font.sizePx));
    textObj->text(text.c_str());
    
    // Get bounding box from ThorVG
    float x, y, w, h;
    textObj->bounds(&x, &y, &w, &h, false);
    
    m.width = w;
    m.height = h;
    
    // Approximate ascent/descent from size (ThorVG may not expose raw metrics)
    // Use typical font proportions: ascent ~80% of size, descent ~20%
    m.ascent = font.sizePx * 0.8f;
    m.descent = font.sizePx * 0.2f;
    m.lineHeight = font.sizePx * 1.2f;
    
    return m;
}
```

### 6B.4.4 Text Rendering (Normative)

```cpp
std::unique_ptr<tvg::Text> FontManager::createTextFill(
    FontHandle handle, const std::string& text,
    float x, float y, TextAlign align, TextBaseline baseline, uint32_t color)
{
    if (!m_fonts.valid(handle)) return nullptr;
    LoadedFont& font = m_fonts.get(handle);
    
    auto textObj = tvg::Text::gen();
    textObj->font(font.familyName.c_str(), static_cast<float>(font.sizePx));
    textObj->text(text.c_str());
    
    // Apply fill color (extract ARGB)
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    textObj->fill(r, g, b, a);
    
    // Calculate position adjustments for align/baseline
    float offsetX = 0, offsetY = 0;
    calculateTextOffset(textObj.get(), align, baseline, font.sizePx, offsetX, offsetY);
    
    // Apply transform for position
    textObj->translate(x + offsetX, y + offsetY);
    
    return textObj;
}

void FontManager::calculateTextOffset(tvg::Text* textObj, TextAlign align, 
                                       TextBaseline baseline, int sizePx,
                                       float& offsetX, float& offsetY) {
    // Get text bounds for alignment
    float bx, by, bw, bh;
    textObj->bounds(&bx, &by, &bw, &bh, false);
    
    // Horizontal alignment
    switch (align) {
        case TextAlign::Left:
        case TextAlign::Start:
            offsetX = 0;
            break;
        case TextAlign::Center:
            offsetX = -bw / 2.0f;
            break;
        case TextAlign::Right:
        case TextAlign::End:
            offsetX = -bw;
            break;
    }
    
    // Vertical baseline
    float ascent = sizePx * 0.8f;  // Approximate
    float descent = sizePx * 0.2f;
    
    switch (baseline) {
        case TextBaseline::Top:
            offsetY = ascent;
            break;
        case TextBaseline::Middle:
            offsetY = (ascent - descent) / 2.0f;
            break;
        case TextBaseline::Alphabetic:
            offsetY = 0;  // Baseline is at y
            break;
        case TextBaseline::Bottom:
            offsetY = -descent;
            break;
    }
}
```

### 6B.4.5 Command Execution for Text

```cpp
void Canvas2DExecutor::executeFillText(const CommandData& cmd) {
    if (!m_currentFont) return;
    
    auto textObj = m_fontManager->createTextFill(
        m_currentFont, cmd.text.text,
        cmd.text.x, cmd.text.y,
        m_currentState.textAlign, m_currentState.textBaseline,
        m_currentState.fillColor
    );
    
    if (!textObj) return;
    
    // Apply current transform
    textObj->transform(m_currentState.transform);
    
    // Apply global alpha
    textObj->opacity(static_cast<uint8_t>(m_currentState.globalAlpha * 255));
    
    // Apply blend mode
    textObj->blend(m_currentState.blendMode);
    
    // Apply clipping if any
    if (m_currentState.clipShape) {
        auto clipClone = m_currentState.clipShape->duplicate();
        textObj->composite(std::move(clipClone), tvg::CompositeMethod::ClipPath);
    }
    
    m_canvas->push(std::move(textObj));
}

void Canvas2DExecutor::executeStrokeText(const CommandData& cmd) {
    if (!m_currentFont) return;
    
    // For stroke text, create shape from text outlines
    auto textShape = tvg::Shape::gen();
    
    LoadedFont& font = m_fontManager->getFont(m_currentFont);
    
    // ThorVG Text doesn't directly support stroke;
    // Alternative: use tvg::Text with fill, then apply stroke effect
    // Or: Render text to a temporary canvas and extract paths
    
    // Simplified approach for v0.1: use fill with hollow effect
    auto textObj = tvg::Text::gen();
    textObj->font(font.familyName.c_str(), static_cast<float>(font.sizePx));
    textObj->text(cmd.text.text);
    
    // Apply stroke color as fill (stroke text is less common)
    uint8_t a = (m_currentState.strokeColor >> 24) & 0xFF;
    uint8_t r = (m_currentState.strokeColor >> 16) & 0xFF;
    uint8_t g = (m_currentState.strokeColor >> 8) & 0xFF;
    uint8_t b = m_currentState.strokeColor & 0xFF;
    textObj->fill(r, g, b, a);
    
    float offsetX = 0, offsetY = 0;
    m_fontManager->calculateTextOffset(textObj.get(), 
        m_currentState.textAlign, m_currentState.textBaseline,
        font.sizePx, offsetX, offsetY);
    
    textObj->translate(cmd.text.x + offsetX, cmd.text.y + offsetY);
    textObj->transform(m_currentState.transform);
    textObj->opacity(static_cast<uint8_t>(m_currentState.globalAlpha * 255));
    textObj->blend(m_currentState.blendMode);
    
    m_canvas->push(std::move(textObj));
}
```

### 6B.4.6 Supported Font Formats (Normative)

ThorVG supports the following font formats:

| Format | Extension | Required |
|--------|-----------|----------|
| TrueType | `.ttf` | YES |
| OpenType | `.otf` | YES |
| TrueType Collection | `.ttc` | RECOMMENDED |
| Web Open Font | `.woff` | NO (v0.1) |

The runtime MUST support TTF and OTF. Other formats are optional.

---

## 6B.5 Command Execution Engine (Normative)

### 6B.5.1 Executor Architecture

```cpp
class Canvas2DExecutor {
public:
    Canvas2DExecutor(tvg::SwCanvas* canvas, ResourceManager* resources);
    
    void execute(const Canvas2DCommandBuffer& buffer);
    
private:
    void executeCommand(const CommandData& cmd);
    
    // State management
    void pushState();
    void popState();
    
    // Current path management
    void beginNewPath();
    void finalizePath();  // Called before fill/stroke
    
    // Apply current state to a shape
    void applyStateToShape(tvg::Shape* shape);
    
private:
    tvg::SwCanvas* m_canvas;
    ResourceManager* m_resources;
    
    std::stack<CanvasState> m_stateStack;
    CanvasState m_currentState;
    
    std::unique_ptr<tvg::Shape> m_currentPath;
    tvg::Point m_currentPoint;  // For quadratic conversion
};
```

### 6B.5.2 Execution Loop (Normative)

```cpp
void Canvas2DExecutor::execute(const Canvas2DCommandBuffer& buffer) {
    // Reset to default state
    m_stateStack = {};
    m_currentState = CanvasState::defaultState();
    m_currentPath = nullptr;
    
    for (size_t i = 0; i < buffer.size(); ++i) {
        executeCommand(buffer[i]);
    }
    
    // Finalize: sync and draw
    m_canvas->sync();
}

void Canvas2DExecutor::executeCommand(const CommandData& cmd) {
    switch (cmd.cmd) {
    case GfxCommand::Save:
        m_stateStack.push(m_currentState);
        break;
        
    case GfxCommand::Restore:
        if (!m_stateStack.empty()) {
            m_currentState = m_stateStack.top();
            m_stateStack.pop();
        }
        break;
        
    case GfxCommand::SetTransform:
        m_currentState.transform = toThorVG(cmd.transform.a, cmd.transform.b,
                                             cmd.transform.c, cmd.transform.d,
                                             cmd.transform.e, cmd.transform.f);
        break;
        
    case GfxCommand::BeginPath:
        m_currentPath = tvg::Shape::gen();
        break;
        
    case GfxCommand::MoveTo:
        if (m_currentPath) {
            m_currentPath->moveTo(cmd.point2.x, cmd.point2.y);
            m_currentPoint = {cmd.point2.x, cmd.point2.y};
        }
        break;
        
    case GfxCommand::LineTo:
        if (m_currentPath) {
            m_currentPath->lineTo(cmd.point2.x, cmd.point2.y);
            m_currentPoint = {cmd.point2.x, cmd.point2.y};
        }
        break;
        
    case GfxCommand::Fill:
        if (m_currentPath) {
            auto shape = m_currentPath->duplicate();
            shape->fill(m_currentState.fillColor);
            applyStateToShape(shape.get());
            m_canvas->push(std::move(shape));
        }
        break;
        
    case GfxCommand::Stroke:
        if (m_currentPath) {
            auto shape = m_currentPath->duplicate();
            shape->stroke(m_currentState.strokeColor);
            shape->stroke(m_currentState.lineWidth);
            shape->stroke(m_currentState.lineJoin);
            shape->stroke(m_currentState.lineCap);
            applyStateToShape(shape.get());
            m_canvas->push(std::move(shape));
        }
        break;
        
    // ... handle all other commands
    
    default:
        break;
    }
}
```

---

## 6B.6 Surface Pool and Upload System (Normative)

### 6B.6.1 Surface Management

```cpp
class SurfacePool {
public:
    SurfaceHandle createSurface(int w, int h);
    void freeSurface(SurfaceHandle s);
    
    tvg::SwCanvas* getSurface(SurfaceHandle s);
    void* getBuffer(SurfaceHandle s);  // Raw pixel buffer for GPU upload
    
    int getWidth(SurfaceHandle s);
    int getHeight(SurfaceHandle s);
    
private:
    struct Surface {
        std::unique_ptr<tvg::SwCanvas> canvas;
        std::vector<uint32_t> buffer;  // BGRA or RGBA
        int width, height;
        bool dirty;
    };
    
    HandlePool<Surface> m_surfaces;
};
```

### 6B.6.2 GPU Upload Strategy

```cpp
class CanvasUploader {
public:
    // Double-buffered upload to avoid GPU stalls
    void scheduleUpload(SurfaceHandle surface, const void* pixels, int w, int h);
    
    // Get GPU texture for compositing (may be previous frame if upload pending)
    GPU_Texture* getTextureForCompositing(SurfaceHandle surface);
    
    // Called after GPU has finished with textures
    void frameComplete();
    
private:
    // Ring buffer of staging buffers
    static constexpr int UPLOAD_FRAMES = 2;
    
    struct UploadBuffer {
        GPU_Texture* stagingTexture;
        GPU_Texture* finalTexture;
        bool uploadPending;
    };
    
    std::array<std::unordered_map<SurfaceHandle, UploadBuffer>, UPLOAD_FRAMES> m_frames;
    int m_currentFrame = 0;
};
```

---

## 6B.7 Compliance Checklist (Chapter 6B)

An implementation is compliant with Chapter 6B if it:

1. Implements retained mode command recording for all `gfx.*` API calls during `draw()`.
2. Executes the command buffer against ThorVG after `draw()` returns.
3. Correctly maps all ARCANEE API functions to ThorVG equivalents as specified.
4. Implements quadratic-to-cubic curve conversion.
5. Implements Canvas-style arc semantics.
6. Manages the transform state stack compatible with `save()/restore()`.
7. Applies clip paths using ThorVG's composite ClipPath method.
8. Implements gradient support with correct stop and spread mode mapping.
9. Provides font/text rendering via FreeType integration or equivalent.
10. Uses double-buffered GPU upload to avoid rendering stalls.

---

*End of Chapter 6B*
