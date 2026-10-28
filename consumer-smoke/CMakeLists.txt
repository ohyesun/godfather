cmake_minimum_required(VERSION 3.20)
project(zg335_consumer C)

find_package(zg335 CONFIG REQUIRED)

add_executable(zg335_consumer main.c)
target_link_libraries(zg335_consumer PRIVATE zg335::zg335_portable)
