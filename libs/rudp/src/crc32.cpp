#include "crc32.h"

#include <array>

#include "packet.h"      // for HEADER_SIZE and CRC_OFFSET

namespace Protocol {

// table[b] = what processing the single byte b does to the CRC.
// 0xEDB88320 is the polynomial 0x04C11DB7 written in "reflected" bit order.
static const std::array<uint32_t, 256>& table()
{
    static const std::array<uint32_t, 256> t = [] {
        std::array<uint32_t, 256> result{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int bit = 0; bit < 8; ++bit)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            result[i] = c;
        }
        return result;
    }();
    return t;
}

uint32_t crc32Update(uint32_t state, const uint8_t* data, size_t length)
{
    const auto& t = table();
    for (size_t i = 0; i < length; ++i)
        state = t[(state ^ data[i]) & 0xFFu] ^ (state >> 8);
    return state;
}

uint32_t crc32(const uint8_t* data, size_t length)
{
    return ~crc32Update(CRC32_INIT, data, length);          // final XOR 0xFFFFFFFF
}

uint32_t datagramCrc(const uint8_t* datagram, size_t length)
{
    static const uint8_t zeros[4] = {0, 0, 0, 0};

    uint32_t state = CRC32_INIT;
    state = crc32Update(state, datagram, CRC_OFFSET);                          // bytes 0..43
    state = crc32Update(state, zeros, sizeof(zeros));                          // CRC field counted as 0
    state = crc32Update(state, datagram + HEADER_SIZE, length - HEADER_SIZE);  // payload
    return ~state;
}

} // namespace Protocol
