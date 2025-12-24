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
    script/BindingHelpers.h
    script/BindingHelpers.cpp
    script/ScriptEngine.cpp
    script/api/SysBinding.cpp
    script/api/FsBinding.cpp
    script/api/GfxBinding.cpp
    script/api/AudioBinding.cpp
    script/api/InputBinding.cpp
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
    audio/AudioManager.cpp
)

set(INPUT_SOURCES
    input/InputManager.cpp
)

set(APP_SOURCES
    app/main.cpp
    app/Runtime.cpp
    app/Workbench.cpp
)

add_library(vfs STATIC
    ${VFS_SOURCES}
)
target_include_directories(vfs PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${PHYSFS_INCLUDE_DIRS}
)
target_link_libraries(vfs PRIVATE
    ${PHYSFS_LIBRARIES}
)
target_compile_features(vfs PUBLIC cxx_std_17)

add_library(common INTERFACE)
target_include_directories(common INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated"
)

# Link common to vfs
target_link_libraries(vfs PRIVATE common)



add_library(arcanee_core STATIC
    ${PLATFORM_SOURCES}
    ${RUNTIME_SOURCES}
    ${SCRIPT_SOURCES}
    ${RENDER_SOURCES}
    ${AUDIO_SOURCES}
    ${INPUT_SOURCES}
    app/Runtime.cpp
    app/Workbench.cpp
)

target_link_libraries(arcanee_core PUBLIC
    vfs
    common
    SDL2::SDL2
    squirrel_static
    sqstdlib_static
    Diligent-GraphicsEngineVk-static
    Diligent-GraphicsEngineOpenGL-static
    Diligent-GraphicsEngineOpenGL-static
    Diligent-GraphicsTools
    Diligent-Imgui
    thorvg_static
    ${LIBOPENMPT_LIBRARIES}
)

target_include_directories(arcanee_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated"
    "${squirrel_SOURCE_DIR}/include"
    ${SDL2_INCLUDE_DIRS}
    ${PHYSFS_INCLUDE_DIRS}
    ${LIBOPENMPT_INCLUDE_DIRS}
)

target_include_directories(arcanee_core SYSTEM PUBLIC
    "${diligentcore_SOURCE_DIR}"
    "${diligenttools_SOURCE_DIR}"
)

target_compile_features(arcanee_core PUBLIC cxx_std_17)

# Optional feature defines
if(LIBOPENMPT_FOUND)
    target_compile_definitions(arcanee_core PUBLIC HAS_LIBOPENMPT)
endif()

# Warnings
if(MSVC)
    target_compile_options(arcanee_core PRIVATE /W4 /WX)
else()
    target_compile_options(arcanee_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Diligent ImGui
target_link_libraries(arcanee_core PRIVATE Diligent-Imgui)

# ImGui Source from DiligentTools ThirdParty
set(IMGUI_DIR "${diligenttools_SOURCE_DIR}/ThirdParty/imgui")
target_sources(arcanee_core PRIVATE "${IMGUI_DIR}/backends/imgui_impl_sdl.cpp")
target_sources(arcanee_core PRIVATE "${IMGUI_DIR}/imgui_demo.cpp")
target_include_directories(arcanee_core SYSTEM PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

add_executable(arcanee
    app/main.cpp
)

target_link_libraries(arcanee PRIVATE arcanee_core)

# Install executable
install(TARGETS arcanee RUNTIME DESTINATION bin)
