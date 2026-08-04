include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(event_bus_tests test_event_bus.cpp)
target_link_libraries(event_bus_tests
    PRIVATE
        portfolio::event_bus
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(event_bus_tests)
