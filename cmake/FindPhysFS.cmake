# FindPhysFS.cmake
# Find the PhysFS library (filesystem abstraction for games)
#
# This module defines:
#   PHYSFS_FOUND        - True if PhysFS was found
#   PHYSFS_INCLUDE_DIRS - PhysFS include directories
#   PHYSFS_LIBRARIES    - PhysFS libraries

find_path(PHYSFS_INCLUDE_DIR
    NAMES physfs.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
)

find_library(PHYSFS_LIBRARY
    NAMES physfs physfs-static
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        /opt/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PhysFS
    DEFAULT_MSG
    PHYSFS_LIBRARY
    PHYSFS_INCLUDE_DIR
)

if(PhysFS_FOUND)
    set(PHYSFS_INCLUDE_DIRS ${PHYSFS_INCLUDE_DIR})
    set(PHYSFS_LIBRARIES ${PHYSFS_LIBRARY})
    mark_as_advanced(PHYSFS_INCLUDE_DIR PHYSFS_LIBRARY)
endif()
