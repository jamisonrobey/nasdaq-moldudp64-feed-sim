find_package(GTest 1.17.0 QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found on system, fetching v1.17.0...")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
    )
    FetchContent_MakeAvailable(googletest)
else()
    message(STATUS "Found system GTest: ${GTest_VERSION}")
endif()

add_executable(unit-tests
    tests/itch_timestamp_test.cpp
    tests/mold_packet_builder_test.cpp
    tests/mold_io_read_message_test.cpp
    tests/mold_retransmission_buffer_test.cpp
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
