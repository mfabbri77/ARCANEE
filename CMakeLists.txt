# ARCANEE - Modern Fantasy Console
# Copyright (C) 2025 Michele Fabbri
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.

cmake_minimum_required(VERSION 3.16)

project(ARCANEE
    VERSION 0.1.0
    DESCRIPTION "Modern Fantasy Console Runtime"
    LANGUAGES C CXX)

# Enforce C++17 Standard (Normative Requirement per Chapter 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Setup dependency finding module path if needed
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find required packages
find_package(SDL2 REQUIRED)
find_package(PhysFS REQUIRED)

# FetchContent for Dependencies
include(FetchContent)

# Squirrel Scripting Language
FetchContent_Declare(
    squirrel
    GIT_REPOSITORY https://github.com/albertodemichelis/squirrel.git
    GIT_TAG        v3.2
    PATCH_COMMAND "${CMAKE_SOURCE_DIR}/cmake/patch_squirrel.sh"
)

# Configure Squirrel options
set(DISABLE_DYNAMIC ON CACHE BOOL "Force static build for Squirrel" FORCE)
set(DISABLE_STATIC OFF CACHE BOOL "Enable static build for Squirrel" FORCE)
# Disable installer to prevent export errors
set(SQ_DISABLE_INSTALLER ON CACHE BOOL "Disable Squirrel install rules" FORCE)
set(SQ_DISABLE_HEADER_INSTALLER ON CACHE BOOL "Disable header install" FORCE)
set(SQ_DISABLE_CMAKE_INSTALLER ON CACHE BOOL "Disable cmake config install" FORCE)

FetchContent_MakeAvailable(squirrel)

# Diligent Engine (Core only - Tools/FX have broken relative includes)
set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "Disable Diligent Samples" FORCE)
set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "Disable Diligent Tests" FORCE)
set(DILIGENT_NO_FORMATTING ON CACHE BOOL "Disable clang-format" FORCE)

FetchContent_Declare(
    DiligentCore
    GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentCore.git
    GIT_TAG        v2.5.5
)

FetchContent_MakeAvailable(DiligentCore)

# Workaround for upstream CMake bug in sq/CMakeLists.txt where it tries to alias 'sq' even if static
# We can't easily patch the file, but by disabling dynamic we avoid half the issue.
# The error `alias target "squirrel::interpreter_static" because target "sq" does not already exist`
# suggests a copy-paste error in upstream where it aliases `sq` instead of `sq_static` for the static target.
# Since we only need the library, we can prevent 'sq' app form building at all if possible,
# or just tolerate it.

# Actually, the upstream bug is:
# add_executable(squirrel::interpreter_static ALIAS sq)
# It should be ALIAS sq_static.
# To fix this without patching, we can define the target `sq` manually as a dummy if needed,
# OR simpler: we use PATCH_COMMAND to fix the file.


# Ensure Position Independent Code if linking into shared libs or just for safety
set_property(TARGET squirrel_static PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET sqstdlib_static PROPERTY POSITION_INDEPENDENT_CODE ON)

# Add source directory
add_subdirectory(src)
