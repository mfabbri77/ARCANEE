# - Try to find SDL2
# Once done, this will define
#
#  SDL2_FOUND - system has SDL2
#  SDL2_INCLUDE_DIRS - the SDL2 include directories
#  SDL2_LIBRARIES - link these to use SDL2
#
# This module also creates IMPORTED target SDL2::SDL2

include(FindPackageHandleStandardArgs)

# Search for the header
find_path(SDL2_INCLUDE_DIR
    NAMES SDL.h
    PATH_SUFFIXES include/SDL2 include
    PATHS
    /usr/local
    /usr
    /opt
)

# Search for the library
find_library(SDL2_LIBRARY
    NAMES SDL2
    PATH_SUFFIXES lib64 lib
    PATHS
    /usr/local
    /usr
    /opt
)

# Handle the QUIETly and REQUIRED arguments and set SDL2_FOUND
find_package_handle_standard_args(SDL2
    REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR
)

if(SDL2_FOUND)
    set(SDL2_LIBRARIES ${SDL2_LIBRARY})
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})

    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(SDL2_INCLUDE_DIR SDL2_LIBRARY)
