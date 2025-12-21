#pragma once

#include <squirrel.h>

namespace arcanee::script {

/**
 * @brief Register the gfx.* table for 2D canvas API.
 * @ref specs/Chapter 6 §6.3
 */
void registerGfxBinding(HSQUIRRELVM vm);

} // namespace arcanee::script
