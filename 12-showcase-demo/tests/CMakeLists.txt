include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(showcase_demo_tests test_showcase_demo.cpp)
target_link_libraries(showcase_demo_tests
    PRIVATE
        klib::showcase
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(showcase_demo_tests)
