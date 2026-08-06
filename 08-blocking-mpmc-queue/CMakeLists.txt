cmake_minimum_required(VERSION 3.16)
project(blocking_mpmc_queue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(blocking_mpmc_queue INTERFACE)
add_library(portfolio::blocking_mpmc_queue ALIAS blocking_mpmc_queue)
target_include_directories(blocking_mpmc_queue INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Stable IDE compilation database in the source tree. CMake rewrites
# ${CMAKE_BINARY_DIR}/compile_commands.json on every configure (including
# googletest _deps TUs); clangd often attaches headers to a googlemock TU
# without -I for this project. A source-tree CDB is not overwritten by generate.
set(_gtest_include "${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include")
set(_ide_compile_commands "${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json")
file(WRITE "${_ide_compile_commands}" "[
  {
    \"directory\": \"${CMAKE_CURRENT_BINARY_DIR}/tests\",
    \"command\": \"/usr/bin/g++ -I${CMAKE_CURRENT_SOURCE_DIR}/include -isystem ${_gtest_include} -std=c++2a -c ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_blocking_mpmc_queue.cpp\",
    \"file\": \"${CMAKE_CURRENT_SOURCE_DIR}/tests/test_blocking_mpmc_queue.cpp\"
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
