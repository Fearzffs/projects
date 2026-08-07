cmake_minimum_required(VERSION 3.16)
project(task_graph LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_library(task_graph INTERFACE)
add_library(portfolio::task_graph ALIAS task_graph)
target_include_directories(task_graph INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(task_graph INTERFACE portfolio::thread_pool)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PortfolioIde.cmake")
