add_executable(unit-tests
    tests/itch_timestamp_test.cpp
    tests/mold_packet_builder_test.cpp
    tests/mold_io_read_message_test.cpp
    tests/mold_retransmission_buffer_test.cpp
    tests/mold_downstream_pacer.test.cpp
)

target_link_libraries(unit-tests PRIVATE
    itch-mold-replay
    GTest::gtest_main
)
target_include_directories(unit-tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

include(GoogleTest)
gtest_discover_tests(unit-tests)
