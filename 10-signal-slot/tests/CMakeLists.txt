include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(signal_slot_tests test_signal_slot.cpp)
target_link_libraries(signal_slot_tests
    PRIVATE
        portfolio::signal_slot
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(signal_slot_tests)
