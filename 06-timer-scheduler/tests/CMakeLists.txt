include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(timer_scheduler_tests test_timer_scheduler.cpp)
target_link_libraries(timer_scheduler_tests
    PRIVATE
        klib::timer_scheduler
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(timer_scheduler_tests)
