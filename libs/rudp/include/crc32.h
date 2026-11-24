// crc32.h -- CRC-32/IEEE 802.3 (the same CRC as zlib and Ethernet).
//
// UDP can deliver a damaged packet. The sender stores a CRC of the whole datagram in the
// header; the receiver recomputes it and drops the packet if the two differ.
// A CRC finds accidental damage only. It is NOT security.
#ifndef CRC32_H
#define CRC32_H

#include <cstddef>
#include <cstdint>

namespace Protocol {

constexpr uint32_t CRC32_INIT = 0xFFFFFFFFu;

// CRC of a whole buffer in one call. crc32("123456789") == 0xCBF43926.
uint32_t crc32(const uint8_t* data, size_t length);

// Incremental form: start with CRC32_INIT, feed pieces, finish with ~state.
uint32_t crc32Update(uint32_t state, const uint8_t* data, size_t length);

// CRC of a complete datagram (48-byte header + payload), counting the 4 CRC bytes
// (offset 44..47) as zero, as the spec requires. `length` must be >= HEADER_SIZE.
uint32_t datagramCrc(const uint8_t* datagram, size_t length);

} // namespace Protocol

#endif
