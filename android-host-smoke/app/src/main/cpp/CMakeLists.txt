cmake_minimum_required(VERSION 3.22.1)
project(zg335host LANGUAGES C)

set(ZG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
set(ZG_PREBUILT "${ZG_ROOT}/prebuilt/${ANDROID_ABI}/lib/libzg335_portable.a")
set(ZG_BRIDGE_DIR "${ZG_ROOT}/bridge")

if(NOT EXISTS "${ZG_PREBUILT}")
    message(FATAL_ERROR "Missing prebuilt static library: ${ZG_PREBUILT}")
endif()

add_library(zg335_portable STATIC IMPORTED)
set_target_properties(zg335_portable PROPERTIES
    IMPORTED_LOCATION "${ZG_PREBUILT}")

add_library(zg335host SHARED
    jni_bridge.c
    "${ZG_BRIDGE_DIR}/src/zg_bridge.c")
target_include_directories(zg335host PRIVATE
    "${ZG_ROOT}/prebuilt/include/zg335"
    "${ZG_BRIDGE_DIR}/include")
target_link_libraries(zg335host PRIVATE
    zg335_portable
    log)

target_compile_features(zg335host PRIVATE c_std_11)
set_target_properties(zg335host PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF)
