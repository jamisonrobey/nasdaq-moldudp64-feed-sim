cmake_minimum_required(VERSION 3.25)
project(itch-mold-replay LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(${PROJECT_NAME} STATIC
    src/server.cpp
    src/mold/retransmission_buffer.cpp
    src/mold/downstream/feed.cpp
    src/mold/downstream/heartbeat.cpp
    src/mold/retransmission/feed.cpp
    src/mold/retransmission/feed_pool.cpp
    src/itch/timestamp.cpp
    src/mold/io.cpp
    src/mold/packet_builder.cpp
    src/util/memory_mapped_file.cpp
    src/util/file_descriptor.cpp
)

add_library(imr::imr ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

target_include_directories(${PROJECT_NAME}
    PUBLIC include
    PRIVATE src
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Werror
    -Wall
    -Wextra
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )
endif()

if(PROJECT_IS_TOP_LEVEL)
    option(BUILD_UNIT_TESTS        "Build unit tests"        OFF)
    option(BUILD_INTEGRATION_TESTS "Build integration tests" OFF)

    option(ENABLE_ASAN             "Enable AddressSanitizer" OFF)
    option(ENABLE_TSAN             "Enable ThreadSanitizer"  OFF)

    option(DEBUG_NO_NETWORK        "Disable network calls"   OFF)
    option(DEBUG_NO_SLEEP          "Disable sleep calls"     OFF)

    if(ENABLE_ASAN AND ENABLE_TSAN)
        message(FATAL_ERROR "ASAN and TSAN are mutually exclusive")
    endif()

    if(ENABLE_ASAN)
        target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize=address,undefined)
        target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address,undefined)
    elseif(ENABLE_TSAN)
        target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize=thread)
        target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=thread)
    endif()

    if(DEBUG_NO_NETWORK)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_NO_NETWORK)
    endif()

    if(DEBUG_NO_SLEEP)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_NO_SLEEP)
    endif()

    if(BUILD_UNIT_TESTS OR BUILD_INTEGRATION_TESTS)
        enable_testing()
        find_package(GTest 1.17.0 QUIET)
        if(NOT GTest_FOUND)
            message(STATUS "GTest not found, fetching v1.17.0...")
            include(FetchContent)
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG        v1.17.0
            )
            FetchContent_MakeAvailable(googletest)
        else()
            message(STATUS "Found system GTest: ${GTest_VERSION}")
        endif()
    endif()

    if(BUILD_UNIT_TESTS)
        add_subdirectory(tests/unit)
    endif()

    if(BUILD_INTEGRATION_TESTS)
        add_subdirectory(tests/integration)
    endif()

endif()
