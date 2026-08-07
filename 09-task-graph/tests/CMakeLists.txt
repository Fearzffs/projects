include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(task_graph_tests test_task_graph.cpp)
target_link_libraries(task_graph_tests
    PRIVATE
        portfolio::task_graph
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(task_graph_tests)
