# NASDAQ MoldUDP64 Feed Simulator
- Implements a [MoldUDP64](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf) server that replays a binary [Nasdaq TotalView-ITCH](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf) file.
- Downstream server multicasts ITCH messages, using the original timestamps for absolute time-based replay pacing.
- Retransmission server for handling client requests for lost or missed messages by sequence number.

## Build
### Requirements
- Linux
- `gcc` >= 14
- `cmake` >= 3.20
- `ninja` 
  - If using the `cmake` presets
```bash
git clone https://github.com/jamisonrobey/itch-mold-replay.git --recursive
cd itch-mold-replay
cmake --preset release
cd build-release && ninja
```
### CMake Presets
```
cmake --list-presets
Available configure presets:

  "debug"                       - Debug
  "release"                     - Release
```
You can use these presets or call cmake yourself.


#### Build flags

You can pass some flags to CMake compiler which are useful for profiling or debugging internals without having to wait for replay timing or network: 
  - `-DDEBUG_NO_NETORK=On` 
    - network send and receive for the server not compiled
  - `-DDEBUG_NO_SLEEP=On`
    - calls to sleep not compiled which disables replay simulation
 - `-DBUILD_TESTS=On`
    - build unit tests
## Usage
### Replay file
- You can obtain TotalView-ITCH data from [emi.nasdaq.com/ITCH/](https://emi.nasdaq.com/ITCH/)
  - You will need to decompress the file before using
### Running
```
./nasdaq-moldudp64-feed-sim [OPTIONS] mold-session itch-replay-file


POSITIONALS:
  mold-session TEXT REQUIRED  MoldUDP64 Session 
  itch-replay-file TEXT:FILE REQUIRED
                              Path to TotalView-ITCH replay file in binary 

OPTIONS:
  -h,     --help              Print this help message and exit 
          --downstream-mcast-group, --group TEXT [239.0.0.1]  
                              Downstream multicast group 
          --downstream-port INT:INT in [1024 - 65535] [3400]  
                              Downstream port 
          --mcast-ttl, --ttl INT:INT in [0 - 255] [1]  
                              Downstream TTL 
          --loopback          Enable downstream loopback 
          --replay-speed, --speed FLOAT [1]  
                              Downstream replay speed 
          --retrans-address TEXT [127.0.0.1]  
                              Retransmission address 
          --retrans-port INT:INT in [1025 - 65535] [3500]  
                              Retransmission port 
          --retrans-buffer-size UINT [4194304]  
                              Retransmission buffer size 
          --retrans-threads, --threads UINT [12]  
                              Number of retransmission threads 
          --start-phase, --start ENUM:value in {close->2,open->1,pre->0} OR {2,1,0} [0]  
                              Market phase to start replay (pre, open, close) 
```
### Example run configurations
```bash
# w/ defaults
./itch_mold_replay SESSION001 path/to/itch_file
# w/ loopback and ttl 2.0
./itch_mold_replay SESSION001 path/to/itch_file --loopback --ttl 2
# w/ replay_speed 50x and starting at market open
./itch_mold_replay SESSION001 --replay-speed 50x --start-phase open 
```
## Useful tools 
### Traffic control (`tc`)
When this server and a client are on the same network or machine, you will observe a reliable connection which is not very useful if you're trying to test handling out-of-order or missing packets with your client. You can use [tc](https://man7.org/linux/man-pages/man8/tc.8.html) to introduce packet loss, reordering, latency jitter etc if desired.
### Wireshark
- ITCH dissectors for Wireshark by the Open Market Initiative for visualizing the output
- The image below is a capture of the multicast feed decoded in Wireshark using the [Nasdaq_Nsm_Equities_TotalView_Itch_v5_0_Dissector.lua](https://github.com/Open-Markets-Initiative/wireshark-lua/blob/main/Nasdaq/Nasdaq_NsmEquities_TotalView_Itch_v5_0_Dissector.lua):
![img.png](https://raw.githubusercontent.com/jamisonrobey/itch-mold-replay/refs/heads/main/img.png)

## More info / Blog Post
[Blog post](https://jamisonrobey.com/nasdaq-moldudp64-totalview-itch-feed-simulator/) about the implementation.
