#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <netinet/in.h>

using namespace std;

namespace Protocol {

// Protocol constants
constexpr uint16_t MAGIC_NUMBER = 0x5032;
constexpr uint8_t PROTOCOL_VERSION = 1;

constexpr size_t HEADER_SIZE = 10;
constexpr size_t MAX_PAYLOAD_SIZE = 1024;
constexpr size_t MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE;

// Message types
enum class PacketType : uint8_t {
    ACK  = 0x01,
    DATA = 0x20
};

// Packet parsing results
enum ParseResult {
    PARSE_OK,
    PARSE_TOO_SHORT,
    PARSE_BAD_MAGIC,
    PARSE_BAD_VERSION,
    PARSE_BAD_LENGTH,
    PARSE_BAD_TYPE
};

// Packet header (logical representation, not sent directly)
struct PacketHeader {
    uint16_t magic = MAGIC_NUMBER;
    uint8_t version = PROTOCOL_VERSION;
    PacketType type = PacketType::DATA;
    uint32_t sequence_number = 0;
    uint16_t payload_length = 0;
};

// Complete packet
struct Packet {
    PacketHeader header;
    string payload;
};

// Reliability state for one peer
struct PeerState {
    uint32_t expected_receive_sequence = 1;

    bool has_pending_packet = false;
    Packet pending_packet;
    sockaddr_in pending_sender{};
};

// Packet serialization
bool encodePacket(
    const Packet& packet,
    uint8_t* output,
    size_t output_capacity,
    size_t& output_length
);

// Packet parsing
ParseResult decodePacket(
    const uint8_t* buffer,
    size_t buffer_length,
    Packet& packet
);

const char* parseResultText(ParseResult result);
const char* packetTypeName(PacketType type);

// Reliable UDP functions
bool sendReliable(
    int socket_fd,
    const sockaddr_in& destination,
    const Packet& packet,
    PeerState& peer_state
);

bool receiveReliable(
    int socket_fd,
    Packet& packet,
    sockaddr_in& sender,
    PeerState& peer_state,
    const sockaddr_in* required_sender = nullptr
);

} 
#endif