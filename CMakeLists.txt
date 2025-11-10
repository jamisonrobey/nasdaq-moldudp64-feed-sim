cmake_minimum_required(VERSION 3.30)
project(nasdaq-moldudp64-feed-sim)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.5.0
)
FetchContent_MakeAvailable(cli11) 

add_subdirectory(external/jamutils)

 add_executable(nasdaq-moldudp64-feed-sim
  src/main.cpp
  src/itch.cpp
  src/mold_udp_64.cpp
  src/message_buffer.cpp
  src/packet_builder.cpp
  src/downstream_server.cpp
  src/retransmission_server.cpp
  src/retransmission_worker.cpp
)

target_link_libraries(nasdaq-moldudp64-feed-sim PRIVATE 
                      CLI11::CLI11
                      jamutils)

if(DEBUG_NO_NETWORK)
    target_compile_definitions(nasdaq-moldudp64-feed-sim PRIVATE DEBUG_NO_NETWORK)
endif()

if(DEBUG_NO_SLEEP)
    target_compile_definitions(nasdaq-moldudp64-feed-sim PRIVATE DEBUG_NO_SLEEP)
endif()


option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
