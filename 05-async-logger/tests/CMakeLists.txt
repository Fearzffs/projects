include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(async_logger_tests test_async_logger.cpp)
target_link_libraries(async_logger_tests
    PRIVATE
        portfolio::async_logger
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(async_logger_tests)
