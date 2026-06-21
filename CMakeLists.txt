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

set(IMR_LOG_LEVEL "0" CACHE STRING "IMR log level: 0=error 1=warn 2=info 3=debug")
set_property(CACHE IMR_LOG_LEVEL PROPERTY STRINGS "0;1;2;3")
target_compile_definitions(${PROJECT_NAME} PRIVATE IMR_LOG_LEVEL=${IMR_LOG_LEVEL})

if(PROJECT_IS_TOP_LEVEL)
    option(BUILD_UNIT_TESTS        "Build unit tests"        OFF)
    option(BUILD_INTEGRATION_TESTS "Build integration tests" OFF)
    option(BUILD_E2E_TESTS         "Build end to end tests"  OFF)

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

        # false positives if using libstdc++ w/ clang++ in <print>.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # add_compile_options()/add_link_options() below are directory-scoped
            # and only apply to targets created *after* this point — fine for the
            # test executables and FetchContent's GTest (both created later in
            # this file), but ${PROJECT_NAME} was already created above, so it
            # needs the flag set directly here or it silently keeps using the
            # default libstdc++.
            target_compile_options(${PROJECT_NAME} PUBLIC -stdlib=libc++)
            target_link_options(${PROJECT_NAME} PUBLIC -stdlib=libc++)

            add_compile_options(-stdlib=libc++)
            add_link_options(-stdlib=libc++)
        endif()
    endif()

    if(DEBUG_NO_NETWORK)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_NO_NETWORK)
    endif()

    if(DEBUG_NO_SLEEP)
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_NO_SLEEP)
    endif()

    if(BUILD_UNIT_TESTS OR BUILD_INTEGRATION_TESTS OR BUILD_E2E_TESTS)
        enable_testing()

        set(_imr_skip_system_gtest FALSE)
        # System-installed GTest is usually compiled with libstdc++, which won't
        # link against libc++-built code (see the TSan/Clang block above).
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND ENABLE_TSAN)
            set(_imr_skip_system_gtest TRUE)
        endif()

        if(NOT _imr_skip_system_gtest)
            find_package(GTest 1.17.0 QUIET)
        endif()

        if(NOT GTest_FOUND)
            message(STATUS "Fetching GTest v1.17.0 from source...")
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

        add_subdirectory(tests/common)
    endif()

    if(BUILD_UNIT_TESTS)
        add_subdirectory(tests/unit)
    endif()

    if(BUILD_INTEGRATION_TESTS)
        add_subdirectory(tests/integration)
    endif()

    if(BUILD_E2E_TESTS)
        add_subdirectory(tests/e2e)
    endif()
endif()
