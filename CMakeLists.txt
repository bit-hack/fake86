cmake_minimum_required(VERSION 2.8)
project(fake86)

find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_definitions("-D_CRT_SECURE_NO_WARNINGS")

file(GLOB SOURCE_COMMON src/common/*.h src/common/*.c)
add_library(lib_common ${SOURCE_COMMON})

file(GLOB SOURCE_EXTERN src/external/*.h src/external/*.c)
add_library(lib_extern ${SOURCE_EXTERN})

file(GLOB SOURCE_AUDIO src/audio/*.h src/audio/*.c)
add_library(lib_audio ${SOURCE_AUDIO})

file(GLOB SOURCE_VIDEO src/video/*.h src/video/*.c)
add_library(lib_video ${SOURCE_VIDEO})

file(GLOB SOURCE_CPU src/80x86/*.h src/80x86/*.c)
add_library(lib_80x86 ${SOURCE_CPU})

file(GLOB SOURCE_F86 src/fake86/*.h src/fake86/*.c)
add_library(lib_fake86 ${SOURCE_F86})

file(GLOB SOURCE_FRONTEND src/frontend/*.h src/frontend/*.c)
add_executable(fake86 ${SOURCE_FRONTEND})

target_link_libraries(fake86
    lib_common
    lib_fake86
    lib_80x86
    lib_extern
    lib_video
    lib_audio
    ${SDL_LIBRARY})
