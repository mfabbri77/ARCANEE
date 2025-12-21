#pragma once

#include "common/Types.h"
#include <squirrel.h>
#include <vector>

namespace arcanee::render {
class Canvas2D;
} // namespace arcanee::render

namespace arcanee::script {

// Host binding config
void setGfxCanvas(render::Canvas2D *canvas);
void setGfxPalette(const std::vector<u32> *palette);

/**
 * @brief Register the gfx.* table for 2D canvas API.
 * @ref specs/Chapter 6 §6.3
 */
void registerGfxBinding(HSQUIRRELVM vm);

} // namespace arcanee::script
