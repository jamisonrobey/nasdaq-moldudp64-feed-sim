`imr` (Nasdaq TotalView-<b>I</b>tch <b>M</b>oldUDP64 <b>R</b>eplay) is a C++23 library that replays [TotalView-ITCH 5.0](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf) [files](#file-format) over [MoldUDP64](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf).

## File format

This library works with files containing TotalView-ITCH 5.0 messages, each prefixed by a 16-bit big-endian value giving the length of the message that follows. This is the format used by official distribution sources such as emi.nasdaq.com.

## Requirements

- Linux 
- `g++` or `clang++` version that supports C++23
- CMake >= 3.25

## Usage

### Add to executable with CMake

As a subdirectory:

```cmake
add_subdirectory(nasdaq-moldudp64-feed-sim)
target_link_libraries(your_target PRIVATE imr::imr)
```

Or via `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(imr GIT_REPOSITORY <repo-url> GIT_TAG <tag>)
FetchContent_MakeAvailable(imr)
target_link_libraries(your_target PRIVATE imr::imr)
```

### Example

Link against `imr::imr` and include `imr/server.h`.

```cpp
#include "imr/server.h"

imr::Server::Config cfg{
    .mapped_itch_file_cfg = {
        .path = "FILE.NADSAQ_ITCH50",
    },
    .packet_builder_cfg = {
        .session = "123456789",
    },
    .downstream_feed_config = {
        .mcast_group = "239.0.0.1",
        .port = 3400,
    },
    .retransmission_feed_config = {
        .address = "127.0.0.1",
        .port = 3500,
    },
};

// Option 1: exceptions
try
{
    imr::Server server(cfg);
    server.start();
    server.wait_for_downstream();
}
catch (const std::exception& ex)
{
    std::println("{}", ex.what());
}

// Option 2: std::expected 
const std::expected res{imr::make_server(cfg)};
if (!res.has_value())
{
    std::println(stderr, "{}", res.error());
}
```

See `include/imr/util/memory_mapped_file.h`, `include/imr/mold/packet_builder.h`,`include/imr/mold/downstream/feed.h`, and `include/imr/mold/retransmission/feed.h`(or their generated documentation pages) for the full set of config options on each struct.

## Log level

Set at configure time via `-DIMR_LOG_LEVEL=N`, compiled in as a
preprocessor definition.
- `0`=error 
- `1`=warn 
- `2`=info 
- `3`=debug 

Levels above configured are compiled out (default 0).

## Complimentary tools

### [`tc`](https://man7.org/linux/man-pages/man8/tc-netem.8.html)

Use the `netem` qdisc to inject artificial packet loss, delay, and reordering

Run the server and client on either side of the impaired link and confirm your client detects gaps and recovers via the retransmission feed.

### Wireshark

Has built in dissectors for MoldUDP64 + TotalView-ITCH 5.0.


## Building standalone 

### Build options (CMake top level)

| Option                    | Default | Description                                    |
|---------------------------|---------|------------------------------------------------|
| `BUILD_UNIT_TESTS`        | `OFF`   | Build unit tests                               |
| `BUILD_INTEGRATION_TESTS` | `OFF`   | Build integration tests                        |
| `BUILD_E2E_TESTS`         | `OFF`   | Build end-to-end tests                         |
| `ENABLE_ASAN`             | `OFF`   | Build with AddressSanitizer + UBSan            |
| `ENABLE_TSAN`             | `OFF`   | Build with ThreadSanitizer (mutually exclusive with ASan) |
| `DEBUG_NO_NETWORK`        | `OFF`   | Disable network calls                          |
| `DEBUG_NO_SLEEP`          | `OFF`   | Disable sleep calls                            |

Example, building with unit tests and ASan:

```sh
$ cmake -B build -DBUILD_UNIT_TESTS=ON -DENABLE_ASAN=ON
$ cmake --build build
$ ctest --test-dir build
```
There are CMake presets:

```sh
$ cmake --list-presets

Available configure presets:

  "debug-gcc"     - Debug (GCC)
  "debug-clang"   - Debug (Clang)
  "release-gcc"   - Release (GCC)
  "release-clang" - Release (Clang)
  "asan-gcc"      - ASan (GCC)
  "asan-clang"    - ASan (Clang)
  "tsan-gcc"      - TSan (GCC)
  "tsan-clang"    - TSan (Clang)
```

## License

MIT — see [LICENSE](https://raw.githubusercontent.com/jamisonrobey/nasdaq-moldudp64-feed-sim/refs/heads/main/LICENSE) for details.

