# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Disable problematic warnings for GoogleTest BEFORE making it available
set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ctor-dtor-privacy -Wno-error")

FetchContent_MakeAvailable(googletest)

# Restore original flags for your test code
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_SAVE}")

# Create test executable
add_executable(tests
    test_message_buffer.cpp
    test_itch_extract_timestamp.cpp
)

# Link against gtest, your source files, and dependencies
target_link_libraries(tests
    PRIVATE
    GTest::gtest_main
    jamutils
)

# Include source directory for headers
target_include_directories(tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Add source files that need to be tested (non-main sources)
target_sources(tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/message_buffer.cpp
    ${PROJECT_SOURCE_DIR}/src/itch.cpp
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(tests)