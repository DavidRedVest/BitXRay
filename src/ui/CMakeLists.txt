# Layer 3: Qt6 UI, the only layer allowed to link both core_parser and
# core_player.
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(bitxray_ui
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

target_link_libraries(bitxray_ui PRIVATE
    Qt6::Widgets
    Qt6::Svg
    core_parser
    core_player
)

get_property(_bitxray_ffmpeg_lib_dir GLOBAL PROPERTY BITXRAY_FFMPEG_LIB_DIR)
if(_bitxray_ffmpeg_lib_dir)
    set_target_properties(bitxray_ui PROPERTIES
        BUILD_RPATH "${_bitxray_ffmpeg_lib_dir}"
        INSTALL_RPATH "${_bitxray_ffmpeg_lib_dir}"
    )
endif()
