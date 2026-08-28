cmake_minimum_required(VERSION 3.21)
project(BitXRay VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MACOSX_RPATH ON)

option(BITXRAY_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(src)

if(BITXRAY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
