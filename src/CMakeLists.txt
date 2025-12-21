# ARCANEE - Modern Fantasy Console
# Copyright (C) 2025 Michele Fabbri
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.

# Create version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/common/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/common/Version.h"
)

# Collect source files by module
set(COMMON_SOURCES
    # Headers only for now (Log.h, Assert.h, Types.h)
)

set(VFS_SOURCES
    vfs/Vfs.cpp
    vfs/VfsImpl.cpp
)

set(RUNTIME_SOURCES
    runtime/Cartridge.cpp
    runtime/Manifest.cpp
)

set(PLATFORM_SOURCES
    platform/Platform.cpp
    platform/Time.cpp
    platform/Window.cpp
)

set(SCRIPT_SOURCES
    script/ScriptEngine.cpp
    script/api/SysBinding.cpp
    script/api/FsBinding.cpp
    script/api/GfxBinding.cpp
)

set(RENDER_SOURCES
    render/RenderDevice.cpp
    render/Framebuffer.cpp
    render/PresentPass.cpp
    render/Canvas2D.cpp
)

set(AUDIO_SOURCES
    audio/AudioDevice.cpp
    audio/ModulePlayer.cpp
    audio/SfxMixer.cpp
)

set(APP_SOURCES
    app/main.cpp
    app/Runtime.cpp
)

add_executable(arcanee
    ${APP_SOURCES}
    ${PLATFORM_SOURCES}
    ${VFS_SOURCES}
    ${RUNTIME_SOURCES}
    ${SCRIPT_SOURCES}
    ${RENDER_SOURCES}
    ${AUDIO_SOURCES}
)

# Include directories
target_include_directories(arcanee PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated"
    "${squirrel_SOURCE_DIR}/include"
    ${SDL2_INCLUDE_DIRS}
    ${PHYSFS_INCLUDE_DIRS}
    ${LIBOPENMPT_INCLUDE_DIRS}
)

# DiligentCore as SYSTEM to suppress warnings from their headers
target_include_directories(arcanee SYSTEM PRIVATE
    "${diligentcore_SOURCE_DIR}"
)

# Link libraries
target_link_libraries(arcanee PRIVATE
    SDL2::SDL2
    ${PHYSFS_LIBRARIES}
    squirrel_static
    sqstdlib_static
    Diligent-GraphicsEngineVk-static
    Diligent-GraphicsEngineOpenGL-static
    Diligent-GraphicsTools
    thorvg_static
    ${LIBOPENMPT_LIBRARIES}
)

# Compilation features
target_compile_features(arcanee PRIVATE cxx_std_17)

# Optional feature defines
if(LIBOPENMPT_FOUND)
    target_compile_definitions(arcanee PRIVATE HAS_LIBOPENMPT)
endif()

# Warnings
if(MSVC)
    target_compile_options(arcanee PRIVATE /W4 /WX)
else()
    target_compile_options(arcanee PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

