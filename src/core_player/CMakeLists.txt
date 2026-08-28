# Layer 2: decode/playback engine. Allowed to depend on FFmpeg — this is the
# only module that may do so. Must never be linked against by core_parser,
# and must never expose a core_parser type in its public headers.

set(FFMPEG_ROOT "/Users/liszt/home/ffmpeg_dynamic_build/ffmpeg_6.1.4" CACHE PATH
    "Path to a prebuilt FFmpeg 6.x dev tree (must contain include/ and lib/)")

find_path(FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h
    PATHS ${FFMPEG_ROOT}/include NO_DEFAULT_PATH)
if(NOT FFMPEG_INCLUDE_DIR)
    find_path(FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h)
endif()
if(NOT FFMPEG_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find FFmpeg headers (searched FFMPEG_ROOT=${FFMPEG_ROOT}). "
                         "Pass -DFFMPEG_ROOT=/path/to/ffmpeg if it's installed elsewhere.")
endif()

set(_bitxray_ffmpeg_libs avcodec avutil swscale)
set(FFMPEG_LIBRARIES "")
foreach(_lib ${_bitxray_ffmpeg_libs})
    find_library(FFMPEG_${_lib}_LIBRARY NAMES ${_lib} PATHS ${FFMPEG_ROOT}/lib NO_DEFAULT_PATH)
    if(NOT FFMPEG_${_lib}_LIBRARY)
        find_library(FFMPEG_${_lib}_LIBRARY NAMES ${_lib})
    endif()
    if(NOT FFMPEG_${_lib}_LIBRARY)
        message(FATAL_ERROR "Could not find FFmpeg library '${_lib}' (searched FFMPEG_ROOT=${FFMPEG_ROOT})")
    endif()
    list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${_lib}_LIBRARY})
endforeach()

add_library(core_player STATIC
    src/VideoDecoder.cpp
)

target_include_directories(core_player
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${FFMPEG_INCLUDE_DIR}
)

target_link_libraries(core_player PRIVATE ${FFMPEG_LIBRARIES})
target_compile_features(core_player PUBLIC cxx_std_17)

# core_player's dylibs live outside any system search path; propagate the
# rpath to whatever executable ends up linking this target so it runs
# without needing DYLD_LIBRARY_PATH set manually.
set_property(GLOBAL PROPERTY BITXRAY_FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
