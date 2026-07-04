cmake_minimum_required(VERSION 3.21)  # 3.21+ for $<TARGET_RUNTIME_DLLS:...>

project(MobilePhotoViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------------
# Qt
# --------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets)
# qt_standard_project_setup() is called later, after the third-party
# dependencies are added, so AUTOMOC/AUTOUIC don't needlessly scan them.

# --------------------------------------------------------------------------
# Bundled dependencies (git submodules under third_party/)
#
# libde265  -> libde265.dll  (HEVC decoder)
# libheif   -> heif.dll       (uses libde265 as its HEVC decoder)
#
# They are configured minimally and built as part of this project, so the
# build order is derived automatically from the target dependency chain
# (app -> heif -> libde265). No hardcoded external paths.
# --------------------------------------------------------------------------

# ---- libde265 -------------------------------------------------------------
set(ENABLE_SDL OFF CACHE BOOL "" FORCE)          # no SDL viewer in dec265
add_subdirectory(third_party/libde265)

# libde265 v1.0.8 bundles an old MSVC-compat extra/stdint.h that shadows the
# real one on recent toolchains, causing "PTRDIFF_MAX/INTMAX_MAX undeclared" in
# the STL headers. Strip extra/ from the include path of every libde265 target
# (library + encoder/SSE object libs) so the complete system <stdint.h> is used;
# the code reaches win32cond.h via a relative path, so extra/ isn't needed on -I.
if(MSVC)
    function(_strip_de265_extra_includes dir)
        get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_t ${_targets})
            get_target_property(_inc ${_t} INCLUDE_DIRECTORIES)
            if(_inc)
                list(FILTER _inc EXCLUDE REGEX "/extra$")
                set_target_properties(${_t} PROPERTIES INCLUDE_DIRECTORIES "${_inc}")
            endif()
        endforeach()
        get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
        foreach(_sub ${_subdirs})
            _strip_de265_extra_includes("${_sub}")
        endforeach()
    endfunction()
    _strip_de265_extra_includes("${CMAKE_CURRENT_SOURCE_DIR}/third_party/libde265")
endif()

# The dec265/enc265 CLI example tools are built unconditionally by libde265 but
# aren't needed here; exclude them from the default build.
foreach(_tool dec265 enc265 sherlock265)
    if(TARGET ${_tool})
        set_target_properties(${_tool} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endforeach()

# Satisfy libheif's find_package(LIBDE265) with the in-tree target instead of a
# system install. Our cmake/FindLIBDE265.cmake (prepended to the module path so
# it takes precedence over libheif's bundled one) reports the in-tree target.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# de265.h #includes the *generated* de265-version.h from the build tree, so
# both the source and binary de265 roots must be on the include path for
# libheif's decoder_libde265.cc (added at parent scope -> inherited by libheif).
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libde265"
    "${CMAKE_CURRENT_BINARY_DIR}/third_party/libde265"
)

# ---- libheif --------------------------------------------------------------
# Only the libde265 HEVC decoder, compiled straight into heif.dll. Everything
# else (other codecs, plugin loading, examples, tests) is disabled to keep the
# build self-contained and matching the previous qmake configuration.
set(ENABLE_PLUGIN_LOADING  OFF CACHE BOOL "" FORCE)
set(WITH_LIBDE265          ON  CACHE BOOL "" FORCE)
set(WITH_LIBDE265_PLUGIN   OFF CACHE BOOL "" FORCE)
set(WITH_X265              OFF CACHE BOOL "" FORCE)
set(WITH_AOM_DECODER       OFF CACHE BOOL "" FORCE)
set(WITH_AOM_ENCODER       OFF CACHE BOOL "" FORCE)
set(WITH_DAV1D             OFF CACHE BOOL "" FORCE)
set(WITH_SvtEnc            OFF CACHE BOOL "" FORCE)
set(WITH_RAV1E             OFF CACHE BOOL "" FORCE)
set(WITH_JPEG_DECODER      OFF CACHE BOOL "" FORCE)
set(WITH_JPEG_ENCODER      OFF CACHE BOOL "" FORCE)
set(WITH_OpenJPEG_ENCODER  OFF CACHE BOOL "" FORCE)
set(WITH_OpenJPEG_DECODER  OFF CACHE BOOL "" FORCE)
set(WITH_EXAMPLES          OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
# libheif unconditionally builds Doxygen API docs when Doxygen is installed;
# suppress that (it also needs graphviz 'dot' and just produces docs we don't want).
set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON)
add_subdirectory(third_party/libheif)

# --------------------------------------------------------------------------
# Application
# --------------------------------------------------------------------------
qt_standard_project_setup()  # enables AUTOMOC / AUTOUIC / AUTORCC for the app

qt_add_executable(MobilePhotoViewer WIN32
    main.cpp
    mainwindow.cpp        mainwindow.h        mainwindow.ui
    imagerenderwidget.cpp imagerenderwidget.h imagerenderwidget.ui
    videowidget.cpp       videowidget.h       videowidget.ui
    heifmanager.cpp       heifmanager.h
    jpegmanager.cpp       jpegmanager.h
    Tools/heifboxes.cpp          Tools/heifboxes.h
    Tools/heifreader.cpp         Tools/heifreader.h
    Tools/imagewithvideobase.cpp Tools/imagewithvideobase.h
    Tools/ramfile.cpp            Tools/ramfile.h
    tinyexif/TinyEXIF.cpp        tinyexif/TinyEXIF.h
    tinyexif/tinyxml2.cpp        tinyexif/tinyxml2.h
)

target_include_directories(MobilePhotoViewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"                                # <jpegmanager.h>, <Tools/...>
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libheif"           # <libheif/heif.h>
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libheif/libheif"   # <libheif/api_structs.h> (private)
    "${CMAKE_CURRENT_BINARY_DIR}/third_party/libheif"           # generated heif_version.h
)

target_link_libraries(MobilePhotoViewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets
    heif
)

# --------------------------------------------------------------------------
# Deployment: put every DLL the exe needs next to it so it runs in place.
# --------------------------------------------------------------------------

# 1) Dependency DLLs (heif.dll, libde265.dll, and anything they pull in).
add_custom_command(TARGET MobilePhotoViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:MobilePhotoViewer> $<TARGET_FILE_DIR:MobilePhotoViewer>
    COMMAND_EXPAND_LISTS
    VERBATIM
)

# 2) Qt runtime (Qt6*.dll, platform/multimedia plugins, FFmpeg DLLs) via windeployqt.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET MobilePhotoViewer POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations --no-compiler-runtime
                    "$<TARGET_FILE:MobilePhotoViewer>"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found; Qt DLLs will not be auto-deployed.")
    endif()
endif()
