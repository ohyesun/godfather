cmake_minimum_required(VERSION 3.22.1)
project(zg_bridge_phase0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(zg_bridge STATIC
    src/zg_bridge.c)
target_include_directories(zg_bridge PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_include_directories(zg_bridge PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src")
target_compile_options(zg_bridge PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(zg_bridge PUBLIC z)

add_executable(zg_bridge_test_abi
    tests/test_abi.c)
target_link_libraries(zg_bridge_test_abi PRIVATE zg_bridge)
target_compile_features(zg_bridge_test_abi PRIVATE c_std_11)
target_compile_options(zg_bridge_test_abi PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(zg_bridge_test_frame
    tests/test_frame.c)
target_include_directories(zg_bridge_test_frame PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(zg_bridge_test_frame PRIVATE zg_bridge z)
target_compile_features(zg_bridge_test_frame PRIVATE c_std_11)
target_compile_options(zg_bridge_test_frame PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(zg_bridge_test_abi PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF)
