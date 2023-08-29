#include <pktbuilder.h>
#include <cassert>
#include <vector>

int main() {
    std::vector<uint8_t> data({
        0xf0, 0xba, 0x00, 0x35, 0x00, 0x24, 0x84, 0x92, 0x00, 0x02, 0x01, 0x00, 
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x67, 0x6f, 0x6f, 
        0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00,0x01
        });
    pktbuilder::UDP::Datagram datagram = pktbuilder::UDP::Datagram::decodeFrom(data);    
    assert(datagram.getSourcePort() == 61626);
    assert(datagram.getDestinationPort() == 53);
    assert(datagram.getPayload().size() == 28);
    assert(datagram.getPayload() == std::vector<uint8_t>({
        0x00, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
        0x00, 0x01, 0x00, 0x01}));
}