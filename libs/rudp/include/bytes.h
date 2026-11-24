#ifndef BYTES_H
#define BYTES_H

#include <cstdint>

namespace Protocol {

inline void writeU16(uint8_t* p, uint16_t v){
    p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>(v & 0xFF);
}

inline void writeU32(uint8_t* p, uint32_t v){
    writeU16(p, static_cast<uint16_t>(v >> 16));
    writeU16(p + 2, static_cast<uint16_t>(v & 0xFFFF));
}

inline void writeU64(uint8_t* p, uint64_t v){
    writeU32(p, static_cast<uint32_t>(v >> 32));
    writeU32(p + 4, static_cast<uint32_t>(v & 0xFFFFFFFFu));
}

inline uint16_t readU16(const uint8_t* p){
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

inline uint32_t readU32(const uint8_t* p){
    return (static_cast<uint32_t>(readU16(p)) << 16) | readU16(p + 2);
}

inline uint64_t readU64(const uint8_t* p){
    return (static_cast<uint64_t>(readU32(p)) << 32) | readU32(p + 4);
}

}

#endif
