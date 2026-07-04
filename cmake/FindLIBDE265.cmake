# Custom FindLIBDE265 for this project.
#
# libde265 is built in-tree via add_subdirectory(third_party/libde265), so there
# is nothing to search for on the system. This module simply reports the in-tree
# CMake target and the include roots libheif's decoder needs.
#
# de265.h #includes the *generated* de265-version.h (produced into the build
# tree), so both the source and binary de265 roots are exposed.
#
# This file is found before libheif's bundled cmake/modules/FindLIBDE265.cmake
# because our cmake/ dir is prepended to CMAKE_MODULE_PATH.

if(TARGET libde265)
    set(LIBDE265_FOUND TRUE)
    set(LIBDE265_VERSION "1.0.8")
    set(LIBDE265_INCLUDE_DIRS
        "${CMAKE_SOURCE_DIR}/third_party/libde265"
        "${CMAKE_BINARY_DIR}/third_party/libde265")
    set(LIBDE265_LIBRARIES libde265)   # CMake target name -> resolved on link
else()
    set(LIBDE265_FOUND FALSE)
    message(FATAL_ERROR "FindLIBDE265: in-tree 'libde265' target not found; "
                        "add_subdirectory(third_party/libde265) must run first.")
endif()
