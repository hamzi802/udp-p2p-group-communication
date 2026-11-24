#include "packet.h"

#include <algorithm>
#include <utility>

#include "bytes.h"
#include "crc32.h"

namespace Protocol {

EncodeResult encodePacket(const Packet& packet,
                          uint8_t* output,
                          size_t output_capacity,
                          size_t& output_length)
{
    output_length = 0;

    if (output == nullptr)
        return EncodeResult::NullOutput;

    const size_t payload_size = packet.payload.size();

    if (payload_size > MAX_PAYLOAD)
        return EncodeResult::PayloadTooLarge;

    if (output_capacity < HEADER_SIZE + payload_size)
        return EncodeResult::OutputTooSmall;

    const PacketHeader& h = packet.header;

    // Every field is written at its own explicit offset. No struct is copied as bytes.
    writeU16(output + 0,  MAGIC);                                    // canonical
    output[2] = VERSION;                                             // canonical
    output[3] = h.type;
    output[4] = h.flags;
    output[5] = h.stream;
    writeU16(output + 6,  h.window);
    writeU32(output + 8,  h.sender_id);
    writeU64(output + 12, h.sender_session);
    writeU64(output + 20, h.receiver_session);
    writeU32(output + 28, h.seq);
    writeU32(output + 32, h.ack);
    writeU32(output + 36, h.sack);
    writeU16(output + 40, static_cast<uint16_t>(payload_size));      // always the real size
    writeU16(output + 42, 0);                                        // reserved: always 0
    writeU32(output + CRC_OFFSET, 0);                                // CRC counted as 0 while computing

    std::copy(packet.payload.begin(), packet.payload.end(), output + HEADER_SIZE);

    const size_t total = HEADER_SIZE + payload_size;
    writeU32(output + CRC_OFFSET, datagramCrc(output, total));       // now store the real CRC

    output_length = total;
    return EncodeResult::Ok;
}

DecodeResult decodePacket(const uint8_t* buffer,
                          size_t buffer_length,
                          Packet& packet)
{
    // Length limits first, so we never read outside the buffer.
    if (buffer == nullptr || buffer_length < HEADER_SIZE)
        return DecodeResult::TooShort;

    if (buffer_length > MAX_DATAGRAM)
        return DecodeResult::TooLong;

    const uint16_t payload_len = readU16(buffer + 40);
    if (static_cast<size_t>(payload_len) != buffer_length - HEADER_SIZE)
        return DecodeResult::BadPayloadLength;

    // CRC last: recompute over the whole datagram (CRC field counted as zero).
    const uint32_t received_crc = readU32(buffer + CRC_OFFSET);
    if (received_crc != datagramCrc(buffer, buffer_length))
        return DecodeResult::BadCrc;

    Packet result;
    PacketHeader& h = result.header;
    h.magic            = readU16(buffer + 0);
    h.version          = buffer[2];
    h.type             = buffer[3];
    h.flags            = buffer[4];
    h.stream           = buffer[5];
    h.window           = readU16(buffer + 6);
    h.sender_id        = readU32(buffer + 8);
    h.sender_session   = readU64(buffer + 12);
    h.receiver_session = readU64(buffer + 20);
    h.seq              = readU32(buffer + 28);
    h.ack              = readU32(buffer + 32);
    h.sack             = readU32(buffer + 36);
    h.payload_len      = payload_len;
    h.reserved         = readU16(buffer + 42);
    h.crc32            = received_crc;
    result.payload.assign(buffer + HEADER_SIZE, buffer + buffer_length);

    packet = std::move(result);
    return DecodeResult::Ok;
}

const char* encodeResultText(EncodeResult result)
{
    switch (result) {
        case EncodeResult::Ok:              return "ok";
        case EncodeResult::PayloadTooLarge: return "payload larger than 1152 bytes";
        case EncodeResult::OutputTooSmall:  return "output buffer too small";
        case EncodeResult::NullOutput:      return "null output buffer";
    }
    return "unknown";
}

const char* decodeResultText(DecodeResult result)
{
    switch (result) {
        case DecodeResult::Ok:               return "ok";
        case DecodeResult::TooShort:         return "datagram shorter than the 48-byte header";
        case DecodeResult::TooLong:          return "datagram longer than 1200 bytes";
        case DecodeResult::BadPayloadLength: return "payload_len does not match datagram size";
        case DecodeResult::BadCrc:           return "CRC mismatch (packet damaged)";
    }
    return "unknown";
}

} // namespace Protocol
