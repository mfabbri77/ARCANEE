cmake_minimum_required(VERSION 3.16)

project(ARCANEE
    VERSION 0.1.0
    DESCRIPTION "Modern Fantasy Console Runtime"
    LANGUAGES CXX)

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

# Add source directory
add_subdirectory(src)
