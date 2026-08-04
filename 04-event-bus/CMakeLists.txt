cmake_minimum_required(VERSION 3.16)
project(event_bus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# First portfolio cross-folder dependency: reuse 03's header-only thread pool.
if(NOT TARGET portfolio::thread_pool)
    get_filename_component(THREAD_POOL_INCLUDE
        "${CMAKE_CURRENT_SOURCE_DIR}/../03-thread-pool/include" ABSOLUTE)
    add_library(thread_pool INTERFACE)
    add_library(portfolio::thread_pool ALIAS thread_pool)
    target_include_directories(thread_pool INTERFACE
        $<BUILD_INTERFACE:${THREAD_POOL_INCLUDE}>
        $<INSTALL_INTERFACE:include>
    )
endif()

add_library(event_bus INTERFACE)
add_library(portfolio::event_bus ALIAS event_bus)
target_include_directories(event_bus INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(event_bus INTERFACE portfolio::thread_pool)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Drop _deps (gtest) entries from compile_commands.json. Otherwise clangd often
# attaches event_bus.hpp to a googlemock TU and reports
# 'thread_pool/thread_pool.hpp' file not found (that command has no -I for 03).
add_custom_target(filter_compile_commands ALL
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILE_COMMANDS_IN=${CMAKE_BINARY_DIR}/compile_commands.json
        -DCOMPILE_COMMANDS_OUT=${CMAKE_BINARY_DIR}/compile_commands.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/filter_compile_commands.cmake
    COMMENT "Filtering compile_commands.json for IDE (drop _deps)"
    VERBATIM
)
