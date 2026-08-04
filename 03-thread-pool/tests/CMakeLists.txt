include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(thread_pool_tests test_thread_pool.cpp)
target_link_libraries(thread_pool_tests
    PRIVATE
        portfolio::thread_pool
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(thread_pool_tests)
