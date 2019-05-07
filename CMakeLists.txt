cmake_minimum_required(VERSION 2.8)
project(fake86)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

file(GLOB SOURCE src/fake86/*.h src/fake86/*.c)
file(GLOB SOURCE src/fake86/*.h src/fake86/*.c)

add_executable(fake86 ${SOURCE})
target_link_libraries(fake86 SDL.lib)
