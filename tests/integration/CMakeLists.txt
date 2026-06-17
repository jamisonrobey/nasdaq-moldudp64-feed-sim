add_executable(integration-tests
    tests/server_test.cpp
    tests/components/mapped_file_test.cpp
    tests/components/downstream_feed_test.cpp
    tests/components/retransmission_feed_test.cpp
)

# todo : probably put this in common somehow just cmake copy into this (does add dir do this ? chekc later )
target_compile_definitions(integration-tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(integration-tests PRIVATE
    itch-mold-replay
    GTest::gtest_main
)
target_include_directories(integration-tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/tests/common
)

include(GoogleTest)
gtest_discover_tests(integration-tests)
