cmake_minimum_required(VERSION 3.16)
project(zg335_probe LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../design" design-build)

add_executable(zg335_probe probe.c)
target_link_libraries(zg335_probe PRIVATE zg335_portable)
target_include_directories(zg335_probe PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../design/include/zg335")
