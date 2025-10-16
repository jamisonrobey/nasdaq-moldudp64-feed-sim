FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(tests
    test_message_buffer.cpp
    test_itch_extract_timestamp.cpp
    ${PROJECT_SOURCE_DIR}/src/message_buffer.cpp
    ${PROJECT_SOURCE_DIR}/src/itch.cpp
)

target_link_libraries(tests PRIVATE GTest::gtest_main jamutils)
target_include_directories(tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(tests)