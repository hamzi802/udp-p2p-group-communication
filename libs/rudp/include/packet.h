// packet.h -- the wire format: constants, the packet as a C++ object, and the codec that
// converts it to/from bytes. The codec never touches a socket and keeps no state.
//
//   offset size field             offset size field
//        0    2 magic                 28    4 seq
//        2    1 version               32    4 ack
//        3    1 type                  36    4 sack
//        4    1 flags                 40    2 payload_len
//        5    1 stream                42    2 reserved
//        6    2 window                44    4 crc32
//        8    4 sender_id             48    . payload (payload_len bytes)
//       12    8 sender_session
//       20    8 receiver_session
#ifndef PACKET_H
#define PACKET_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Protocol {

// ---- constants (protocol.md section 2). Do not change without changing the spec. ----------
constexpr uint16_t MAGIC        = 0x5032;                         // "P2"
constexpr uint8_t  VERSION      = 1;
constexpr size_t   HEADER_SIZE  = 48;
constexpr size_t   MAX_DATAGRAM = 1200;                           // header included
constexpr size_t   MAX_PAYLOAD  = MAX_DATAGRAM - HEADER_SIZE;     // 1152
constexpr size_t   RECV_BUFFER  = 2048;                           // recvfrom buffer
constexpr size_t   CRC_OFFSET   = 44;                             // where the 4 CRC bytes sit

// ---- the packet ---------------------------------------------------------------------------
struct PacketHeader {
    uint16_t magic            = MAGIC;
    uint8_t  version          = VERSION;
    uint8_t  type             = 0;
    uint8_t  flags            = 0;
    uint8_t  stream           = 0;
    uint16_t window           = 0;
    uint32_t sender_id        = 0;
    uint64_t sender_session   = 0;
    uint64_t receiver_session = 0;
    uint32_t seq              = 0;
    uint32_t ack              = 0;
    uint32_t sack             = 0;
    uint16_t payload_len      = 0;
    uint16_t reserved         = 0;
    uint32_t crc32            = 0;
};

struct Packet {
    PacketHeader header;
    std::vector<uint8_t> payload;     // raw bytes (binary-safe, not text)
};

enum class EncodeResult {
    Ok,
    PayloadTooLarge,     // payload longer than MAX_PAYLOAD (1152)
    OutputTooSmall,      // the output buffer cannot hold header + payload
    NullOutput
};

enum class DecodeResult {
    Ok,
    TooShort,            // fewer than 48 bytes: cannot hold a header
    TooLong,             // more than 1200 bytes: larger than any valid datagram
    BadPayloadLength,    // payload_len does not equal (datagram length - 48)
    BadCrc               // CRC in the header does not match the datagram
};

// Writes header + payload into `output` and sets `output_length`.
// Always writes the canonical magic, version, reserved (0), payload_len (= payload.size())
// and crc32 (computed) itself, so a caller cannot produce an inconsistent packet.
EncodeResult encodePacket(const Packet& packet,
                          uint8_t* output,
                          size_t output_capacity,
                          size_t& output_length);

// Reads a received datagram. On Ok, `packet` holds every field. On failure it is untouched.
// Checks: length limits, payload_len consistency, CRC. (Rules on magic/version/type/flags
// are the next stage.)
DecodeResult decodePacket(const uint8_t* buffer,
                          size_t buffer_length,
                          Packet& packet);

const char* encodeResultText(EncodeResult result);
const char* decodeResultText(DecodeResult result);

} // namespace Protocol

#endif
