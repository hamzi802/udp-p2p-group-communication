#pragma once // ELI5: stop this header from being included more than once.

#include <array>   // ELI5: fixed-size containers, used for hashes.
#include <cstdint> // ELI5: integers whose sizes are predictable.
#include <string>  // ELI5: text such as a file's display name.
#include <vector>  // ELI5: growable lists for bytes and hashes.

namespace cn { // ELI5: keep shared data types under the project's name.

using PeerId = std::uint32_t; // ELI5: a small number that names one peer.
using Digest256 = std::array<std::uint8_t, 32>; // ELI5: 32 bytes that make up one SHA-256 hash.
using FileId = Digest256; // ELI5: the whole-file hash also serves as that file's ID.
using Bytes = std::vector<std::uint8_t>; // ELI5: a container for any number of raw bytes.

struct PeerIdentity {
    PeerId id{}; // ELI5: which peer this is.
    std::uint64_t session{}; // ELI5: which run of that peer this is.

    // ELI5: identities match only when the peer number and run number both match.
    friend bool operator==(const PeerIdentity& lhs, const PeerIdentity& rhs) noexcept {
        return lhs.id == rhs.id && lhs.session == rhs.session; // ELI5: both identity parts must be equal.
    }
    friend bool operator!=(const PeerIdentity& lhs, const PeerIdentity& rhs) noexcept {
        return !(lhs == rhs); // ELI5: report that the identities differ when equality is false.
    }
    friend bool operator<(const PeerIdentity& lhs, const PeerIdentity& rhs) noexcept {
        return lhs.id < rhs.id || (lhs.id == rhs.id && lhs.session < rhs.session); // ELI5: sort first by peer number, then by session.
    }
};

struct ChunkKey {
    FileId file_id{}; // ELI5: which file owns this piece.
    std::uint32_t index{}; // ELI5: where the piece sits in that file.

    // ELI5: two chunk keys match when file and position are both the same.
    friend bool operator==(const ChunkKey& lhs, const ChunkKey& rhs) noexcept {
        return lhs.file_id == rhs.file_id && lhs.index == rhs.index; // ELI5: both the file and piece number must match.
    }
    friend bool operator!=(const ChunkKey& lhs, const ChunkKey& rhs) noexcept {
        return !(lhs == rhs); // ELI5: report that the keys differ when equality is false.
    }
    friend bool operator<(const ChunkKey& lhs, const ChunkKey& rhs) noexcept {
        return lhs.file_id < rhs.file_id || // ELI5: sort different files by their hash ID first...
               (lhs.file_id == rhs.file_id && lhs.index < rhs.index); // ELI5: ...then sort pieces by their number within the same file.
    }
};

struct FileManifest {
    FileId file_id{}; // ELI5: hash that identifies the complete file.
    std::string display_name; // ELI5: human-friendly name; never use it as a storage path.
    std::uint64_t file_size{}; // ELI5: total number of bytes in the file.
    std::uint32_t chunk_size{}; // ELI5: size of each normal piece.
    std::uint32_t chunk_count{}; // ELI5: number of pieces in the file.
    std::uint32_t final_chunk_size{}; // ELI5: exact size of the last piece.
    std::vector<Digest256> chunk_hashes; // ELI5: one checksum per piece, in file order.
};

namespace message_type {
inline constexpr std::uint8_t catalog_request = 0x30; // ELI5: ask another peer for its file list.
inline constexpr std::uint8_t file_announce = 0x31; // ELI5: tell peers the basic facts about one file.
inline constexpr std::uint8_t manifest_page = 0x32; // ELI5: carry part of a file's chunk hashes.
inline constexpr std::uint8_t have_page = 0x33; // ELI5: carry part of a peer's chunk checklist.
} // namespace message_type

} // namespace cn: shared peer, file, chunk, and message types are declared.
