function(imr_add_test_executable name)
    add_executable(${name} ${ARGN})
    target_compile_definitions(${name} PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    )
    target_link_libraries(${name} PRIVATE
        itch-mold-replay
        GTest::gtest_main
    )
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests/common
    )
    include(GoogleTest)
    gtest_discover_tests(${name})
endfunction()
