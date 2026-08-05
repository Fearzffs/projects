include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(arena_allocator_tests test_arena.cpp)
target_link_libraries(arena_allocator_tests
    PRIVATE
        portfolio::arena_allocator
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(arena_allocator_tests)
