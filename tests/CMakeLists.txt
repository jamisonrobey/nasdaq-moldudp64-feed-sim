FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(tests
    test_retransmission_buffer.cpp
    test_itch_extract_timestamp.cpp
    test_temporal_pacer.cpp
    test_packet_builder.cpp

    ${PROJECT_SOURCE_DIR}/src/retransmission_buffer.cpp
    ${PROJECT_SOURCE_DIR}/src/itch.cpp
    ${PROJECT_SOURCE_DIR}/src/temporal_pacer.h
    ${PROJECT_SOURCE_DIR}/src/packet_builder.cpp
    ${PROJECT_SOURCE_DIR}/src/mold_udp_64.cpp
)

target_link_libraries(tests PRIVATE GTest::gtest_main jamutils)
target_include_directories(tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(tests)
