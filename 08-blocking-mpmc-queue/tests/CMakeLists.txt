include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(blocking_mpmc_queue_tests test_blocking_mpmc_queue.cpp)
target_link_libraries(blocking_mpmc_queue_tests
    PRIVATE
        klib::blocking_mpmc_queue
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(blocking_mpmc_queue_tests)
