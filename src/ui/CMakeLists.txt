# Layer 3: Qt6 UI, the only layer allowed to link both core_parser and
# core_player.
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(BITXRAY_UI_SOURCES
    main.cpp
    MainWindow.h
    MainWindow.cpp
    NaluListModel.h
    NaluListModel.cpp
    HexView.h
    HexView.cpp
    SyntaxTreeModel.h
    SyntaxTreeModel.cpp
    VideoDecodeEngine.h
    VideoDecodeEngine.cpp
    VideoPreviewWidget.h
    VideoPreviewWidget.cpp
    VideoPlayerWidget.h
    VideoPlayerWidget.cpp
    PlaybackWindow.h
    PlaybackWindow.cpp
    resources.qrc
)

# MACOSX_BUNDLE below is a no-op on non-Apple platforms (still produces a
# plain executable there) — only Apple needs the icon wired into the bundle's
# Resources/ via MACOSX_PACKAGE_LOCATION.
if(APPLE)
    set(BITXRAY_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/resources/BitXRay.icns")
    set_source_files_properties(${BITXRAY_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND BITXRAY_UI_SOURCES ${BITXRAY_ICNS})
endif()

# WIN32 selects the GUI subsystem on Windows (no-op elsewhere) — without it,
# the linker defaults to the console subsystem and every launch pops up a
# console window behind the actual Qt UI.
add_executable(bitxray_ui WIN32 MACOSX_BUNDLE ${BITXRAY_UI_SOURCES})

target_link_libraries(bitxray_ui PRIVATE
    Qt6::Widgets
    Qt6::Svg
    core_parser
    core_player
)

if(APPLE)
    set_target_properties(bitxray_ui PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "BitXRay"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.davidredvest.bitxray"
        MACOSX_BUNDLE_ICON_FILE "BitXRay.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
endif()

get_property(_bitxray_ffmpeg_lib_dir GLOBAL PROPERTY BITXRAY_FFMPEG_LIB_DIR)
if(_bitxray_ffmpeg_lib_dir)
    set_target_properties(bitxray_ui PROPERTIES
        BUILD_RPATH "${_bitxray_ffmpeg_lib_dir}"
        INSTALL_RPATH "${_bitxray_ffmpeg_lib_dir}"
    )
endif()
