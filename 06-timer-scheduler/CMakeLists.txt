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

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PortfolioIde.cmake")
