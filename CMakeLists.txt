cmake_minimum_required(VERSION 4.2)
project(itch-mold-replay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_UNIT_TESTS "Enable testing" OFF)
option(ENABLE_ASAN "Enable ASan + UBSan" OFF)
option(ENABLE_TSAN "Enable TSan" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ASAN and TSAN are mutually exclusive")
endif()

include(GNUInstallDirs)
include(FetchContent)

add_library(${PROJECT_NAME} STATIC
    src/itch/timestamp.cpp
    src/mold/io.cpp
    src/mold/packet_builder.cpp
    src/mold/retransmission_buffer.cpp
    src/util/memory_mapped_file.cpp
)
target_include_directories(${PROJECT_NAME}
    PUBLIC  include
    PRIVATE src
)

set(PROJECT_WARNINGS
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND PROJECT_WARNINGS -Werror)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND PROJECT_WARNINGS
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )
endif()

target_compile_options(${PROJECT_NAME} PRIVATE ${PROJECT_WARNINGS})
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -O3 -march=native -flto=auto)
        target_link_options(${PROJECT_NAME} PRIVATE -flto=auto)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -O3 -march=native -flto)
        target_link_options(${PROJECT_NAME} PRIVATE -flto)
    endif()
endif()

if(ENABLE_ASAN)
    target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize=address,undefined)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address,undefined)
elseif(ENABLE_TSAN)
    target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize=thread)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=thread)
endif()

if(BUILD_UNIT_TESTS)
    enable_testing()
    add_subdirectory(tests/unit)
endif()
