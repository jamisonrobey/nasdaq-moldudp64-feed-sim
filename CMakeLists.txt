cmake_minimum_required(VERSION 3.30)
project(nasdaq-moldudp64-feed-sim)

include(FetchContent)

FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.5.0
)
FetchContent_MakeAvailable(cli11) 

add_subdirectory(external/jamutils)


add_executable(nasdaq-moldudp64-feed-sim src/main.cpp 
                                         src/server/message_buffer.cpp)
target_link_libraries(nasdaq-moldudp64-feed-sim PRIVATE 
                      CLI11::CLI11
                      jamutils)


option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()