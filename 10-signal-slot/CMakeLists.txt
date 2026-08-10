cmake_minimum_required(VERSION 3.16)
project(signal_slot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(signal_slot INTERFACE)
add_library(portfolio::signal_slot ALIAS signal_slot)
target_include_directories(signal_slot INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PortfolioIde.cmake")
