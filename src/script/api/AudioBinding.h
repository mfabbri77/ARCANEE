#pragma once

#include <squirrel.h>

// Forward decl
namespace arcanee::vfs {
class IVfs;
}

namespace arcanee::script {

/**
 * @brief Register audio.* Squirrel bindings.
 */
void registerAudioBinding(HSQUIRRELVM vm);

/**
 * @brief Set the VFS instance for audio file loading.
 */
void setAudioVfs(arcanee::vfs::IVfs *vfs);

} // namespace arcanee::script
