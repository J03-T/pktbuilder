# PKTBuilder
## A simple, intuitive C++ packet crafting library.

PKTBuilder makes it easy to construct network packets at any layer of the 
network stack using a simple, clean syntax. 

It can construct packets from an ever increasing number of network protocols, 
from the application layer to the link layer. 

It uses sane default values, and lower level protocols are able to infer 
values from the higher level protocols contained within them. For example,
an IPv4 packet will automatically set its protocol number to the correct value
when a Layer 4 protocol is added on top. 

PKTBuilder currently only supports Linux, but Windows support will be added 
very soon.

## Installation

While precompiled static binaries are available, it is recommended that you 
compile PKTBuilder from source using CMake. PKTBuilder has no dependencies,
but requires a compiler with support for C++20 (you can check which compilers
are supported [here](https://en.cppreference.com/w/cpp/compiler_support/20)).

To build PKTBuilder you will also need CMake 3.22 or greater.
### Compiling PKTBuilder

To compile PKTBuilder, clone the repository with:
```
git clone https://github.com/joedthomas2005/pktbuilder
```
Then move into the `pktbuilder` directory with:
```
cd pktbuilder
```
Configure CMake with:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release .
```
Build PKTBuilder with:
```
cmake --build build
```
Inside the `build` directory will be a file called `libpktbuilder.a`
(or `libpktbuilder.so` if it was built as a shared library). 

### Linking to PKTBuilder

To use PKTBuilder in your program, first this library file must be placed in a 
directory where your compiler can link to it. This could either be in the system-wide
default library path (usually `/usr/lib` on Linux) or a local directory for your
program. 

If the library is not placed in your default system library path, you must add 
the directory containing it to your compiler's library path. If you are using
g++, add `-L<PATH TO FOLDER CONTAINING LIBRARY>` to your compiler command (e.g. 
if the headers were stored in your project directory under `lib/pktbuilder`
then you would add `-L./lib/pktbuilder` to your g++ command). If your project
is using CMake then add a line to your `CMakeLists.txt` which adds the directory
to your linker path (`link_directories(<PATH TO FOLDER> <OTHER LIBRARY PATHS>...)`).

In order to use the library you will need to link to it at compile time. In order
to do this using g++ you should add `-lpktbuilder` to your compiler command. To 
link to the library using CMake you should add a line to your `CMakeLists.txt`
add it to the libraries for your CMake target (`target_link_libraries(<YOUR TARGET>
pktbuilder <OTHER LIBRARIES...>)`).

You will also need the PKTBuilder headers found in this repository. Simply copy 
the entire contents of this repository's `include` directory and add it to 
somewhere in your include path. This could either be the system-wide include path
(usually `/usr/include` on Linux), or your program's local include path.

Make sure the directory you have placed the headers in is added to your 
compiler's include path. If your compiler is g++, add `-I<PATH TO HEADERS>` to
your compiler command (e.g. if the headers were stored in your project directory
under `lib/pktbuilder/include` then you would add `-I./lib/pktbuilder/include` to 
your compile command), If your project is using CMake then add a line to your 
`CMakeLists.txt` which adds the directory to your include path (
`include_directories(<PATH TO HEADERS> <OTHER INCLUDE DIRECTORIES...>)`).

In order to use pktbuilder in your project, simply `#include <pktbuilder.h>`.
## Getting Started

PKTBuilder uses a simple syntax to represent network packets, in which layers of 
protocols are separated using the `|` character in descending order (i.e. the 
first protocol will be contained within the next).

This allows protocol stacks to be represented visually with the higher level
protocols at the top and the lower level protocols at the bottom like so:

```c++
auto packet = std::string("some data") |
              pktbuilder::UDPDatagram(8080) |
              pktbuilder::IPv4Packet({192, 168, 1, 1}) |
              pktbuilder::EthernetFrame({0x01, 0x23, 0x45, 0x67, 0x89, 0xab});
```

In this example, it can be seen that the "application" data (the string 
"hello world") is at the top, and is then encapsulated within a UDP Datagram 
with destination port `8080` (the UDP source port is optional),
which is encapsulated within an IPv4 packet with 
destination address `192.168.1.1` which is then itself contained within an 
Ethernet frame with destination address `01:23:45:67:89:ab`.

Notice also that the source hardware address for the ethernet frame and the 
source IP address for the IPv4 packet were not required. This is because
pktbuilder is able to automatically fill them based on your device's MAC
and IP address. 

The UDP datagram would also usually require the source and destination IP 
addresses in order to calculate the checksum, but these are set automatically
by the IPv4 packet below it. 

Furthermore, an Ethernet frame is required to contain an `ethertype` field which 
indicates which layer 3 protocol is encapsulated within it and IPv4 packet is 
required to contain a `protocol number` field which indicates the layer 4 protocol.
These also did not need to be supplied as they are automatically filled by the 
upper level protocols (IPv4 and UDP respectively). 

PKTBuilder does not currently handle sending packets, and so to send the packet 
you must first call the `build()` method on it to get an `std::vector` containing 
the raw packet bytes:
```c++
std::vector<uint8_t> data = packet.build();
```
This can then be sent using whatever means you wish, for example, here I use the 
`pcap` library to send the raw ethernet frame directly from the `eth0` network
interface.
```c++
    char errbuf[PCAP_BUF_SIZE];
    pcap_t* handle = pcap_open_live("eth0", BUFSIZ, 1, 1000, errbuf);
    pcap_sendpacket(handle, data.data(), data.size());
```
If you were not using such low level protocols, you could also use a BSD 
socket, for example:
```c++
auto packet = std::array<uint8_t, 4>({0xab, 0xff, 0xde, 0x1}) |
              pktbuilder::TCPPacket(80, {192, 168, 1, 1}, 200, 4000000000,
                  pktbuilder::TCPFlag::PSH | pktbuilder::TCPFlag::RST, 1024, {});

std::vector<uint8_t> data = packet.build();

int s = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);
struct sockaddr_in addr = { 0 };
addr.sin_family = AF_INET;
inet_aton("192.168.1.1", &addr.sin_addr);
connect(s, (sockaddr*)&addr, sizeof(addr));
send(s, data.data(), data.size(), 0);
```

This example constructs a TCP packet from port 200 to port 80 (the
destination IP address does need to be specified this time as there is no
IPv4 packet to supply it. The source address can be set automatically, however)
 containing the payload `ab ff de 01`  with a sequence number of `4000000000`, 
with the `PSH` and `RST` flags set, a window size of `1024` and no TCP options.
The TCP packet is then sent using a raw layer 3 IPv4 socket to `192.168.1.1`.

PKTBuilder is extremely flexible, and allows for effectively any combination of
protocols in any order to suit any use case. Any protocol which accepts a payload
can have any other protocol contained within it. There are certain protocols which
do not accept a payload, such as `ARP`, and these can only ever be the very top
protocol in the stack, but aside from this limitation, any protocol can be added 
to any other protocol in any order. 

Want an AppleTalk ethertype Ethernet frame contained in an ICMP echo request in
a UDP datagram?
```c++
auto packet = pktbuilder::EthernetFrame({0xab, 0xcd, 0xef, 0x12, 0x34, 0x56},
                                        pktbuilder::EtherType::AppleTalk,
                                        {0xde, 0xad, 0xbe, 0xef, 0, 0}) |
                                        pktbuilder::ICMPPacket(8, 0, {0, 0, 0, 0}) |
                                        pktbuilder::UDPDatagram(1024, 800);
```

This flexibility allows any part of the network stack to be switched out for 
any other without affecting the higher layers. 
DNS over UDP and DNS over TCP are only 1 line of code different (Disclaimer: 
DNS is not currently implemented, but I'm working on it).