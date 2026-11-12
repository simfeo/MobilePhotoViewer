qt_add_executable(heif_bench
    heif_bench.cpp
    ${PROJECT_SOURCE_DIR}/src/formats/heifmanager.cpp
    ${PROJECT_SOURCE_DIR}/src/formats/heifpreview.cpp
    ${PROJECT_SOURCE_DIR}/src/formats/imagewithvideobase.cpp
    ${PROJECT_SOURCE_DIR}/src/formats/heifreader.cpp
    ${PROJECT_SOURCE_DIR}/src/formats/heifboxes.cpp
    ${PROJECT_SOURCE_DIR}/src/formats/ramfile.cpp
    ${PROJECT_SOURCE_DIR}/src/formats/jpegrepair.cpp
    ${PROJECT_SOURCE_DIR}/third_party/TinyEXIF/TinyEXIF.cpp
    ${PROJECT_SOURCE_DIR}/third_party/tinyxml2/tinyxml2.cpp
)

target_include_directories(heif_bench PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/third_party/TinyEXIF"
    "${PROJECT_SOURCE_DIR}/third_party/tinyxml2"
    "${PROJECT_SOURCE_DIR}/third_party/libheif/libheif/api"
    "${PROJECT_BINARY_DIR}/third_party/libheif"
)

target_link_libraries(heif_bench PRIVATE Qt6::Core Qt6::Gui heif)
if(MPV_FFMPEG_HEVC)
    target_compile_definitions(heif_bench PRIVATE MPV_FFMPEG_HEVC)
endif()

if(APPLE)
    target_link_libraries(heif_bench PRIVATE
        "-framework ImageIO" "-framework CoreGraphics" "-framework CoreFoundation")
    # libheif looks for libde265 via @loader_path, which only works inside the app
    # bundle; in the build tree the two sit in different dirs, so add both here.
    set_target_properties(heif_bench PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:heif>;$<TARGET_FILE_DIR:de265>")
endif()
