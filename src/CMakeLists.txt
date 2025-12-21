# src/CMakeLists.txt

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
)

# Include directories
target_include_directories(arcanee PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated"
    ${SDL2_INCLUDE_DIRS}
    ${PHYSFS_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(arcanee PRIVATE
    ${SDL2_LIBRARIES}
    ${PHYSFS_LIBRARIES}
)

# Compilation features
target_compile_features(arcanee PRIVATE cxx_std_17)

# Warnings
if(MSVC)
    target_compile_options(arcanee PRIVATE /W4 /WX)
else()
    target_compile_options(arcanee PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

