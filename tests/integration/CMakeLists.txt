add_executable(integration-tests
    tests/server_test.cpp
    tests/components/mapped_file_test.cpp
    tests/components/downstream_feed_test.cpp
    tests/components/retransmission_feed_test.cpp
)

configure_file(
    data/mmap_test_file.txt
    ${CMAKE_CURRENT_BINARY_DIR}/mmap_test_file.txt
    COPYONLY
)

target_compile_definitions(integration-tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(integration-tests PRIVATE
    itch-mold-replay
    GTest::gtest_main
)
target_include_directories(integration-tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

include(GoogleTest)
gtest_discover_tests(integration-tests)
