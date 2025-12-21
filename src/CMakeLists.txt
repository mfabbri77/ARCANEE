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

    # Render
    render/RenderDevice.cpp
    script/api/FsBinding.cpp
    script/api/FsBinding.h
)

set(APP_SOURCES
    app/main.cpp
    app/Runtime.cpp
)

# Define the main executable
add_executable(arcanee
    ${APP_SOURCES}
    ${PLATFORM_SOURCES}
    ${VFS_SOURCES}
    ${RUNTIME_SOURCES}
    ${SCRIPT_SOURCES}
)

# Include directories
target_include_directories(arcanee PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated"
    "${squirrel_SOURCE_DIR}/include"
    ${SDL2_INCLUDE_DIRS}
    ${PHYSFS_INCLUDE_DIRS}
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
)

# Compilation features
target_compile_features(arcanee PRIVATE cxx_std_17)

# Warnings
if(MSVC)
    target_compile_options(arcanee PRIVATE /W4 /WX)
else()
    target_compile_options(arcanee PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

