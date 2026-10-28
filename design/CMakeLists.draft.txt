# Draft only. Do not treat this file as a verified platform build.
cmake_minimum_required(VERSION 3.16)
project(zg335_portable LANGUAGES C)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Two roots are required: the frozen legacy component root and the A1-A5
# adapter package live in different trees. Do not vendor or modify either.
if(NOT DEFINED ZG_LEGACY_ROOT)
    message(FATAL_ERROR "Set ZG_LEGACY_ROOT to the frozen C11 component root")
endif()
if(NOT DEFINED ZG_ADAPTER_ROOT)
    message(FATAL_ERROR "Set ZG_ADAPTER_ROOT to the A1-A5 adapter package")
endif()

set(ZG335_SOURCES
    "${ZG_LEGACY_ROOT}/normal116-fullstate-clean-C11-author-20261027-v2/zg_normal116_fullstate.c"
    "${ZG_LEGACY_ROOT}/node-provider52-prospective-C11-SOURCE431-author-v1/zg_node_provider52_candidate.c"
    "${ZG_LEGACY_ROOT}/node-copy32-prospective-C11-SOURCE431-author-v1/zg_node_copy32_candidate.c"
    "${ZG_LEGACY_ROOT}/wrapper154-portable-library-SOURCE453-author-v1/src/zg_wrapper154_candidate.c"
    "${ZG_LEGACY_ROOT}/range76e0-C11-author-v1/zg_range76e0.c"
    "${ZG_LEGACY_ROOT}/range101e0-raw-frame-C11-SOURCE-author-20261026-v1/zg_range101e0_raw_frame.c"
    "${ZG_LEGACY_ROOT}/normal116-rich-lower-raw335-clean-C11-author-20261027-v1/zg_copy335_rich116.c"
    "${ZG_LEGACY_ROOT}/copy335-raw-normal-frame-clean-C11-author-C-20261027-v1/zg_copy335_raw_normal_frame.c"
    "${ZG_ADAPTER_ROOT}/zg116la_common.c"
    "${ZG_ADAPTER_ROOT}/zg116la_raw52.c"
    "${ZG_ADAPTER_ROOT}/zg116la_fe0_range.c"
    "${ZG_ADAPTER_ROOT}/zg116la_e4_range.c"
    "${ZG_ADAPTER_ROOT}/zg116la_adapter.c"
)

set(ZG335_PUBLIC_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg335_portable.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg116la_adapter.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg116la_common.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg116la_raw52.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg116la_fe0_range.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg116la_e4_range.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg_copy335_rich116.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg_copy335_raw_normal_frame.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg_normal116_fullstate.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg_node_provider52_candidate.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg_node_copy32_candidate.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg_wrapper154_candidate.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg_range76e0.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/zg335/zg_range101e0_raw_frame.h"
)

add_library(zg335_portable STATIC ${ZG335_SOURCES})
target_compile_features(zg335_portable PUBLIC c_std_11)
set_target_properties(zg335_portable PROPERTIES
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    PUBLIC_HEADER "${ZG335_PUBLIC_HEADERS}")

target_include_directories(zg335_portable
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/zg335>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zg335>"
    PRIVATE
    "${ZG_ADAPTER_ROOT}"
    "${ZG_LEGACY_ROOT}/normal116-fullstate-clean-C11-author-20261027-v2"
    "${ZG_LEGACY_ROOT}/normal116-rich-lower-raw335-clean-C11-author-20261027-v1"
    "${ZG_LEGACY_ROOT}/copy335-raw-normal-frame-clean-C11-author-C-20261027-v1"
    "${ZG_LEGACY_ROOT}/range101e0-raw-frame-C11-SOURCE-author-20261026-v1"
    "${ZG_LEGACY_ROOT}/range76e0-C11-author-v1"
    "${ZG_LEGACY_ROOT}/wrapper154-portable-library-SOURCE453-author-v1/include"
    "${ZG_LEGACY_ROOT}/node-provider52-prospective-C11-SOURCE431-author-v1"
    "${ZG_LEGACY_ROOT}/node-copy32-prospective-C11-SOURCE431-author-v1")

if(MSVC)
    target_compile_options(zg335_portable PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(zg335_portable PRIVATE
        -Wall -Wextra -Wpedantic -Werror -fvisibility=hidden)
endif()

install(TARGETS zg335_portable
    EXPORT zg335Targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/zg335"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/zg335")
install(EXPORT zg335Targets
    NAMESPACE zg335::
    FILE zg335Targets.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zg335")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/zg335Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/zg335Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zg335")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/zg335ConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/zg335Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/zg335ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/zg335")
