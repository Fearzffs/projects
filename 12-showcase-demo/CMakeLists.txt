cmake_minimum_required(VERSION 3.16)
project(showcase_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(klib_ensure_interface target_name folder_name)
    if(NOT TARGET klib::${target_name})
        get_filename_component(_inc
            "${CMAKE_CURRENT_SOURCE_DIR}/../${folder_name}/include" ABSOLUTE)
        add_library(${target_name} INTERFACE)
        add_library(klib::${target_name} ALIAS ${target_name})
        target_include_directories(${target_name} INTERFACE
            $<BUILD_INTERFACE:${_inc}>
            $<INSTALL_INTERFACE:include>
        )
    endif()
endfunction()

klib_ensure_interface(thread_pool 03-thread-pool)
klib_ensure_interface(spsc_ring_buffer 02-spsc-ring-buffer)
klib_ensure_interface(ring_buffer 01-ring-buffer)
klib_ensure_interface(blocking_mpmc_queue 08-blocking-mpmc-queue)
klib_ensure_interface(arena_allocator 07-arena-allocator)
klib_ensure_interface(fsm 11-fsm)
klib_ensure_interface(signal_slot 10-signal-slot)

if(NOT TARGET klib::async_logger)
    klib_ensure_interface(async_logger 05-async-logger)
    target_link_libraries(async_logger INTERFACE klib::spsc_ring_buffer)
endif()

if(NOT TARGET klib::event_bus)
    klib_ensure_interface(event_bus 04-event-bus)
    target_link_libraries(event_bus INTERFACE klib::thread_pool)
endif()

if(NOT TARGET klib::timer_scheduler)
    klib_ensure_interface(timer_scheduler 06-timer-scheduler)
    target_link_libraries(timer_scheduler INTERFACE klib::thread_pool)
endif()

if(NOT TARGET klib::task_graph)
    klib_ensure_interface(task_graph 09-task-graph)
    target_link_libraries(task_graph INTERFACE klib::thread_pool)
endif()

find_package(Threads REQUIRED)

add_library(showcase_lib
    src/metrics.cpp
    src/demo.cpp
)
add_library(klib::showcase ALIAS showcase_lib)
target_include_directories(showcase_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(showcase_lib
    PUBLIC
        klib::thread_pool
        klib::async_logger
        klib::event_bus
        klib::timer_scheduler
        klib::task_graph
        klib::signal_slot
        klib::fsm
        klib::blocking_mpmc_queue
        klib::ring_buffer
        klib::arena_allocator
        Threads::Threads
)

add_executable(showcase_demo src/main.cpp)
target_link_libraries(showcase_demo PRIVATE klib::showcase Threads::Threads)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PortfolioIde.cmake")
