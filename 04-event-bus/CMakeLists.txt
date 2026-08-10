cmake_minimum_required(VERSION 3.16)
project(event_bus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# First portfolio cross-folder dependency: reuse 03's header-only thread pool.
if(NOT TARGET klib::thread_pool)
    get_filename_component(THREAD_POOL_INCLUDE
        "${CMAKE_CURRENT_SOURCE_DIR}/../03-thread-pool/include" ABSOLUTE)
    add_library(thread_pool INTERFACE)
    add_library(klib::thread_pool ALIAS thread_pool)
    target_include_directories(thread_pool INTERFACE
        $<BUILD_INTERFACE:${THREAD_POOL_INCLUDE}>
        $<INSTALL_INTERFACE:include>
    )
endif()

add_library(event_bus INTERFACE)
add_library(klib::event_bus ALIAS event_bus)
target_include_directories(event_bus INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(event_bus INTERFACE klib::thread_pool)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PortfolioIde.cmake")
