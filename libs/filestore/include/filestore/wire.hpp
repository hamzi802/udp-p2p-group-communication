#pragma once // ELI5: prevent duplicate inclusion.

#include "common/file_types.hpp" // ELI5: reuse shared IDs, hashes, and byte lists.

#include <cstddef> // ELI5: use standard size numbers.
#include <cstdint> // ELI5: use predictable-width numbers in network messages.
#include <stdexcept> // ELI5: report malformed messages with an exception.
#include <vector> // ELI5: hold pages of hashes and bytes.

namespace cn::filestore::wire { // ELI5: group the file-storage message formats together.

inline constexpr std::size_t max_payload_size = 1152; // ELI5: keep each message within this byte limit.
inline constexpr std::uint32_t manifest_hashes_per_page = 34; // ELI5: fit this many 32-byte checksums on one page.
inline constexpr std::uint32_t have_bits_per_page = 8192; // ELI5: let one presence page describe 8192 pieces.

class ProtocolError : public std::runtime_error { // ELI5: name errors caused by invalid network data.
public:
    using std::runtime_error::runtime_error; // ELI5: reuse the standard error's message constructor.
};

struct ManifestPage { // ELI5: one packet-sized slice of a file's checksum list.
    FileId file_id{}; // ELI5: say which file this page describes.
    std::uint32_t page_index{}; // ELI5: say which page number this is.
    std::uint32_t page_count{}; // ELI5: say how many pages complete the list.
    std::uint32_t first_chunk_index{}; // ELI5: say which piece's checksum comes first.
    std::vector<Digest256> hashes; // ELI5: hold the checksums carried by this page.
};

struct HavePage { // ELI5: one compact checklist page showing pieces a peer owns.
    FileId file_id{}; // ELI5: say which file the checklist belongs to.
    std::uint64_t generation{}; // ELI5: identify the version of the peer's checklist.
    std::uint32_t page_index{}; // ELI5: say which checklist page this is.
    std::uint32_t page_count{}; // ELI5: say how many pages complete the checklist.
    std::uint32_t first_chunk_index{}; // ELI5: say which piece number starts this page.
    std::uint16_t bit_count{}; // ELI5: count the pieces described by this page.
    Bytes bitmap; // ELI5: use one bit per piece to keep the checklist small.
};

Bytes encode_catalog_request(); // ELI5: make the bytes that ask another peer for its file list.
void decode_catalog_request(const Bytes& payload); // ELI5: check that an incoming file-list request is well formed.

Bytes encode_file_announce(const FileManifest& manifest); // ELI5: pack a file's basic details into network bytes.
FileManifest decode_file_announce(const Bytes& payload); // ELI5: unpack and check basic file details from network bytes.

std::vector<ManifestPage> make_manifest_pages(const FileManifest& manifest); // ELI5: split all checksums into message-sized pages.
Bytes encode_manifest_page(const ManifestPage& page); // ELI5: pack one checksum page into network bytes.
ManifestPage decode_manifest_page(const Bytes& payload); // ELI5: unpack and check one received checksum page.

std::vector<HavePage> make_have_pages(const FileId& file_id, // ELI5: identify the file in the checklist.
                                      std::uint64_t generation, // ELI5: label which checklist version is being sent.
                                      const std::vector<std::uint8_t>& present_by_chunk); // ELI5: turn piece ownership into compact pages.
Bytes encode_have_page(const HavePage& page); // ELI5: pack one ownership page into network bytes.
HavePage decode_have_page(const Bytes& payload); // ELI5: unpack and check one received ownership page.
bool have_page_contains(const HavePage& page, std::uint32_t chunk_index); // ELI5: ask whether this page says a peer has one piece.

} // namespace cn::filestore::wire: the message formats and helpers are declared.
