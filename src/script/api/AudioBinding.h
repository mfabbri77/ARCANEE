#pragma once

#include <squirrel.h>

namespace arcanee::script {

/**
 * @brief Register audio.* Squirrel bindings.
 */
void registerAudioBinding(HSQUIRRELVM vm);

} // namespace arcanee::script
