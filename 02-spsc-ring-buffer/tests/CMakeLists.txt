include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(spsc_ring_buffer_tests test_spsc_ring_buffer.cpp)
target_link_libraries(spsc_ring_buffer_tests
    PRIVATE
        klib::spsc_ring_buffer
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(spsc_ring_buffer_tests)
