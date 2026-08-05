cmake_minimum_required(VERSION 3.16)
project(timer_scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Reuse 03's header-only thread pool.
get_filename_component(THREAD_POOL_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/../03-thread-pool/include" ABSOLUTE)
if(NOT TARGET portfolio::thread_pool)
    add_library(thread_pool INTERFACE)
    add_library(portfolio::thread_pool ALIAS thread_pool)
    target_include_directories(thread_pool INTERFACE
        $<BUILD_INTERFACE:${THREAD_POOL_INCLUDE}>
        $<INSTALL_INTERFACE:include>
    )
endif()

add_library(timer_scheduler INTERFACE)
add_library(portfolio::timer_scheduler ALIAS timer_scheduler)
target_include_directories(timer_scheduler INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(timer_scheduler INTERFACE portfolio::thread_pool)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Stable IDE compilation database in the source tree. CMake rewrites
# ${CMAKE_BINARY_DIR}/compile_commands.json on every configure (including
# googletest _deps TUs); clangd then attaches portfolio headers to a googlemock
# command and reports missing cross-folder includes. A source-tree CDB is not
# overwritten by generate. Include gtest paths so TEST(...) expands as a macro
# (otherwise clangd treats the suite name as a type: "Unknown type name
# 'TimerScheduler'").
set(_gtest_include "${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include")
set(_ide_compile_commands "${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json")
file(WRITE "${_ide_compile_commands}" "[
  {
    \"directory\": \"${CMAKE_CURRENT_BINARY_DIR}/tests\",
    \"command\": \"/usr/bin/g++ -I${CMAKE_CURRENT_SOURCE_DIR}/include -I${THREAD_POOL_INCLUDE} -isystem ${_gtest_include} -std=c++2a -c ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_timer_scheduler.cpp\",
    \"file\": \"${CMAKE_CURRENT_SOURCE_DIR}/tests/test_timer_scheduler.cpp\"
  }
]
")

# Also keep build/compile_commands.json filtered after each build.
add_custom_target(filter_compile_commands ALL
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILE_COMMANDS_IN=${CMAKE_BINARY_DIR}/compile_commands.json
        -DCOMPILE_COMMANDS_OUT=${CMAKE_BINARY_DIR}/compile_commands.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/filter_compile_commands.cmake
    COMMENT "Filtering compile_commands.json for IDE (drop _deps)"
    VERBATIM
)
