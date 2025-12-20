# src/CMakeLists.txt

# Create version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/common/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/common/Version.h"
)

# Define the main executable
add_executable(arcanee
    app/main.cpp
    app/Runtime.cpp
    platform/Window.cpp
)

# Include directories
target_include_directories(arcanee PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated"
    ${SDL2_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(arcanee PRIVATE
    ${SDL2_LIBRARIES}
)

# Compilation features
target_compile_features(arcanee PRIVATE cxx_std_17)

# Warnings
if(MSVC)
    target_compile_options(arcanee PRIVATE /W4 /WX)
else()
    target_compile_options(arcanee PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
