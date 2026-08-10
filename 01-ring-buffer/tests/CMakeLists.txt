include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ring_buffer_tests test_ring_buffer.cpp)
target_link_libraries(ring_buffer_tests
    PRIVATE
        klib::ring_buffer
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(ring_buffer_tests)
