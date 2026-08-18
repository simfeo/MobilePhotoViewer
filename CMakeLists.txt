cmake_minimum_required(VERSION 3.21)  # 3.21+ for $<TARGET_RUNTIME_DLLS:...>

# Version: defaults to 1.0.0 for local builds; CI passes -DAPP_VERSION=<tag>.
set(APP_VERSION "1.0.0" CACHE STRING "Application version (major.minor.patch)")
# project() accepts only a strictly numeric version, so strip any pre-release
# suffix (e.g. "1.0.0-rc1", "0.0.0-test") down to the leading numeric part.
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" APP_VERSION_NUMERIC "${APP_VERSION}")
if(NOT APP_VERSION_NUMERIC)
    set(APP_VERSION_NUMERIC "0.0.0")
endif()
project(MobilePhotoViewer
    VERSION ${APP_VERSION_NUMERIC}
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------------
# Qt
# --------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets OpenGL OpenGLWidgets LinguistTools)

# macOS 14+ SDKs removed the long-deprecated AGL framework, but an OpenGL CMake
# config still lists it as a link dependency, breaking the link on Apple Silicon
# ("ld: framework 'AGL' not found"). The app uses modern QOpenGLWidget
# (OpenGL.framework), not AGL, so strip the stale reference from the imported
# targets that may carry it. Safe no-op on targets/platforms that don't.
if(APPLE)
    foreach(_agl_target
            OpenGL::GL Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Gui
            Qt6::GuiPrivate Qt6::OpenGLPrivate)
        if(TARGET ${_agl_target})
            foreach(_prop INTERFACE_LINK_LIBRARIES IMPORTED_LINK_INTERFACE_LIBRARIES)
                get_target_property(_libs ${_agl_target} ${_prop})
                if(_libs)
                    list(FILTER _libs EXCLUDE REGEX "(-framework )?AGL(\\.framework)?$")
                    set_target_properties(${_agl_target} PROPERTIES ${_prop} "${_libs}")
                endif()
            endforeach()
        endif()
    endforeach()
endif()
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
# Codecs added since 1.17 that default ON -- keep them off (they'd need x264 /
# OpenH264 and pull external dependencies we don't want).
set(WITH_X264              OFF CACHE BOOL "" FORCE)
set(WITH_OpenH264_DECODER  OFF CACHE BOOL "" FORCE)
# Extra features/tools that default ON in newer libheif; none are needed for a
# minimal HEVC-decode build and each drags in more dependencies.
set(WITH_LIBSHARPYUV        OFF CACHE BOOL "" FORCE)
set(WITH_UNCOMPRESSED_CODEC OFF CACHE BOOL "" FORCE)
set(WITH_HEADER_COMPRESSION OFF CACHE BOOL "" FORCE)
set(WITH_GDK_PIXBUF         OFF CACHE BOOL "" FORCE)
set(WITH_EXAMPLES           OFF CACHE BOOL "" FORCE)
set(WITH_EXAMPLE_HEIF_THUMB OFF CACHE BOOL "" FORCE)
set(WITH_EXAMPLE_HEIF_VIEW  OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING           OFF CACHE BOOL "" FORCE)
# libheif unconditionally builds Doxygen API docs when Doxygen is installed;
# suppress that (it also needs graphviz 'dot' and just produces docs we don't want).
set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON)
add_subdirectory(third_party/libheif)

# --------------------------------------------------------------------------
# Application
# --------------------------------------------------------------------------
qt_standard_project_setup()  # enables AUTOMOC / AUTOUIC / AUTORCC for the app

qt_add_executable(MobilePhotoViewer WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp        mainwindow.h        mainwindow.ui
    imagerenderwidget.cpp imagerenderwidget.h imagerenderwidget.ui
    heifmanager.cpp       heifmanager.h
    jpegmanager.cpp       jpegmanager.h
    thumbnailmodel.cpp    thumbnailmodel.h
    panoramawidget.cpp    panoramawidget.h
    depthwidget.cpp       depthwidget.h
    metadatapanel.cpp     metadatapanel.h
    folderbrowserpanel.cpp folderbrowserpanel.h
    locationbar.cpp       locationbar.h
    gridcontextmenu.cpp   gridcontextmenu.h
    settingsdialog.cpp    settingsdialog.h
    appsettings.cpp       appsettings.h
    i18n.cpp              i18n.h
    Tools/heifboxes.cpp          Tools/heifboxes.h
    Tools/heifreader.cpp         Tools/heifreader.h
    Tools/imagewithvideobase.cpp Tools/imagewithvideobase.h
    Tools/ramfile.cpp            Tools/ramfile.h
    # EXIF/XMP parsing (TinyEXIF fork with motion-photo support) + its tinyxml2
    # dependency, compiled straight in from the third_party submodules.
    third_party/TinyEXIF/TinyEXIF.cpp   third_party/TinyEXIF/TinyEXIF.h
    third_party/tinyxml2/tinyxml2.cpp   third_party/tinyxml2/tinyxml2.h
    resources/appicon.qrc               # window icon (Qt resource)
    $<$<BOOL:${WIN32}>:resources/app.rc>
    explorerselectiondelegate.h explorerselectiondelegate.cpp # taskbar/exe icon (Windows)
)

# macOS: the Dock/Finder icon comes from an .icns bundled into the .app and
# named in Info.plist. setWindowIcon() (main.cpp) does NOT set it on macOS.
if(APPLE)
    set(MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.icns")
    target_sources(MobilePhotoViewer PRIVATE ${MACOS_ICON})
    set_source_files_properties(${MACOS_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    set_target_properties(MobilePhotoViewer PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "appicon.icns"
        # Notarization requires a stable bundle identifier and a hardened-runtime
        # signature (applied later by the CI codesign step).
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.simfeo.MobilePhotoViewer"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

# --------------------------------------------------------------------------
# Translations
#
# Compiles translations/*.ts into .qm and embeds them in the binary under
# :/i18n/, where main() picks one up via QTranslator. No files to ship.
#
#   cmake --build <dir> --target update_translations   # rescan tr() -> .ts
#
# To add a language: add translations/MobilePhotoViewer_<code>.ts here and a row
# to AppSettings::availableLanguages().
# --------------------------------------------------------------------------
qt_add_translations(MobilePhotoViewer
    TS_FILES
        translations/MobilePhotoViewer_en.ts
        translations/MobilePhotoViewer_uk.ts
    RESOURCE_PREFIX "/i18n"
)

target_include_directories(MobilePhotoViewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"                                # <jpegmanager.h>, <Tools/...>
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/TinyEXIF"          # <TinyEXIF.h>
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyxml2"          # <tinyxml2.h>
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libheif/libheif/api"  # <libheif/heif.h>, <libheif/heif_cxx.h>
    "${CMAKE_CURRENT_BINARY_DIR}/third_party/libheif"              # generated <libheif/heif_version.h>
)

target_link_libraries(MobilePhotoViewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets
    Qt6::OpenGL Qt6::OpenGLWidgets
    heif
)

# 2) Qt runtime (Qt6*.dll, platform/multimedia plugins, FFmpeg DLLs) via windeployqt.
if(WIN32)
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

elseif(APPLE)
    # --------------------------------------------------------------------------
    # macOS: bundle the Qt frameworks and the heif/libde265 dylibs into the .app
    # (and fix their rpaths) so it runs on machines without a Qt install.
    # macdeployqt follows the app's linked libraries, so heif + libde265 are
    # pulled in automatically -- no manual copy step needed.
    # --------------------------------------------------------------------------
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET MobilePhotoViewer POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                    "$<TARGET_BUNDLE_DIR:MobilePhotoViewer>"
                    -always-overwrite
            VERBATIM
        )
    else()
        message(WARNING "macdeployqt not found; Qt frameworks will not be auto-bundled.")
    endif()
endif()
