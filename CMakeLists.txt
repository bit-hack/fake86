cmake_minimum_required(VERSION 2.8)
project(fake86)

find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_definitions("-D_CRT_SECURE_NO_WARNINGS")

add_subdirectory(src/external)


file(GLOB SOURCE_COMMON
    src/common/*.h
    src/common/*.c)
add_library(lib_common ${SOURCE_COMMON})


file(GLOB SOURCE_AUDIO
    src/audio/*.h
    src/audio/*.c)
add_library(lib_audio ${SOURCE_AUDIO})
target_link_libraries(lib_audio
    lib_nukedopl)


file(GLOB SOURCE_VIDEO
    src/video/*.h
    src/video/*.c)
add_library(lib_video ${SOURCE_VIDEO})


file(GLOB SOURCE_CPU
    src/cpu/*.h
    src/cpu/*.c)
add_library(lib_cpu ${SOURCE_CPU})
target_link_libraries(lib_cpu
    lib_udis86)


file(GLOB SOURCE_F86
    src/fake86/*.h
    src/fake86/*.c)
add_library(lib_fake86 ${SOURCE_F86})


file(GLOB SOURCE_DISK
    src/disk/*.h
    src/disk/*.c)
add_library(lib_disk ${SOURCE_DISK})


file(GLOB SOURCE_FRONTEND
    src/frontend/*.h
    src/frontend/*.c)
add_executable(fake86 ${SOURCE_FRONTEND})


target_link_libraries(fake86
    lib_common
    lib_fake86
    lib_cpu
    lib_video
    lib_audio
    lib_disk
    ${SDL_LIBRARY})


file(GLOB SOURCE_TESTS_OPCODES
    src/tests/opcodes/*.h
    src/tests/opcodes/*.c)
add_executable(tests_opcodes ${SOURCE_TESTS_OPCODES})

target_link_libraries(tests_opcodes
    lib_common
    lib_cpu
    ${SDL_LIBRARY})
