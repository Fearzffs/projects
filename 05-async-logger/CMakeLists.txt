cmake_minimum_required(VERSION 3.16)
project(async_logger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Reuse 02's header-only SPSC ring buffer.
if(NOT TARGET portfolio::spsc_ring_buffer)
    get_filename_component(SPSC_INCLUDE
        "${CMAKE_CURRENT_SOURCE_DIR}/../02-spsc-ring-buffer/include" ABSOLUTE)
    add_library(spsc_ring_buffer INTERFACE)
    add_library(portfolio::spsc_ring_buffer ALIAS spsc_ring_buffer)
    target_include_directories(spsc_ring_buffer INTERFACE
        $<BUILD_INTERFACE:${SPSC_INCLUDE}>
        $<INSTALL_INTERFACE:include>
    )
endif()

add_library(async_logger INTERFACE)
add_library(portfolio::async_logger ALIAS async_logger)
target_include_directories(async_logger INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(async_logger INTERFACE portfolio::spsc_ring_buffer)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Drop _deps (gtest) entries from compile_commands.json. Otherwise clangd often
# attaches async_logger.hpp to a googlemock TU and reports
# 'spsc_ring_buffer/spsc_ring_buffer.hpp' file not found (no -I for 02).
add_custom_target(filter_compile_commands ALL
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILE_COMMANDS_IN=${CMAKE_BINARY_DIR}/compile_commands.json
        -DCOMPILE_COMMANDS_OUT=${CMAKE_BINARY_DIR}/compile_commands.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/filter_compile_commands.cmake
    COMMENT "Filtering compile_commands.json for IDE (drop _deps)"
    VERBATIM
)
