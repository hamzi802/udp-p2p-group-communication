#include "filestore/wire.hpp" // ELI5: get the network-message types and function declarations.

#include "filestore/manifest.hpp" // ELI5: check file details and piece-count limits.

#include <algorithm> // ELI5: copy and compare message data.
#include <limits> // ELI5: guard against values too large for the message format.

namespace cn::filestore::wire { // ELI5: keep message encoding code in its own namespace.
namespace { // ELI5: hide byte-reading helpers from other files.

class Writer { // ELI5: build a network message by adding its fields one by one.
public:
    void u16(std::uint16_t value) {
        bytes_.push_back(static_cast<std::uint8_t>(value >> 8)); // ELI5: add the high byte first.
        bytes_.push_back(static_cast<std::uint8_t>(value)); // ELI5: then add the low byte.
    }
    void u32(std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) bytes_.push_back(static_cast<std::uint8_t>(value >> shift)); // ELI5: split the number into four bytes in a fixed order.
    }
    void u64(std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) bytes_.push_back(static_cast<std::uint8_t>(value >> shift)); // ELI5: split the number into eight bytes in a fixed order.
    }
    void raw(const std::uint8_t* data, std::size_t size) {
        if (size == 0) return; // ELI5: an empty field adds no bytes.
        bytes_.insert(bytes_.end(), data, data + size); // ELI5: append the requested raw bytes.
    }
    void raw(const Bytes& data) { raw(data.data(), data.size()); } // ELI5: append all bytes from a byte list.
    Bytes take() {
        if (bytes_.size() > max_payload_size) throw ProtocolError("M3 payload exceeds 1152 bytes"); // ELI5: refuse messages larger than the protocol allows.
        return std::move(bytes_); // ELI5: hand the completed message bytes to the caller.
    }
private:
    Bytes bytes_; // ELI5: keep all bytes added by this writer.
};

class Reader { // ELI5: take fields out of a message while checking bounds.
public:
    explicit Reader(const Bytes& bytes) : bytes_(bytes) { // ELI5: read from the supplied message without copying it.
        if (bytes.size() > max_payload_size) throw ProtocolError("M3 payload exceeds 1152 bytes"); // ELI5: reject a message over the protocol size limit.
    }
    std::uint16_t u16() {
        require(2); // ELI5: ensure two bytes are available before reading.
        const auto value = static_cast<std::uint16_t>((bytes_[position_] << 8) | bytes_[position_ + 1]);
        position_ += 2; // ELI5: mark the two bytes as used.
        return value; // ELI5: return their decoded number.
    }
    std::uint32_t u32() {
        require(4); // ELI5: ensure four bytes are available.
        std::uint32_t value = 0; // ELI5: start building the decoded number.
        for (int i = 0; i < 4; ++i) value = (value << 8) | bytes_[position_++]; // ELI5: combine four bytes from left to right.
        return value; // ELI5: return the 32-bit number.
    }
    std::uint64_t u64() {
        require(8); // ELI5: ensure eight bytes are available.
        std::uint64_t value = 0; // ELI5: start building the decoded number.
        for (int i = 0; i < 8; ++i) value = (value << 8) | bytes_[position_++]; // ELI5: combine eight bytes from left to right.
        return value; // ELI5: return the 64-bit number.
    }
    void raw(std::uint8_t* output, std::size_t size) {
        require(size); // ELI5: ensure the requested bytes are available.
        std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(position_), size, output); // ELI5: copy them into the caller's buffer.
        position_ += size; // ELI5: move to the next unread byte.
    }
    Bytes bytes(std::size_t size) {
        require(size); // ELI5: check that the requested byte list is present.
        Bytes result(bytes_.begin() + static_cast<std::ptrdiff_t>(position_),
                     bytes_.begin() + static_cast<std::ptrdiff_t>(position_ + size));
        position_ += size; // ELI5: skip over the bytes copied into the result.
        return result; // ELI5: return those message bytes.
    }
    std::string text(std::size_t size) {
        require(size); // ELI5: check that the requested text bytes are present.
        std::string result(bytes_.begin() + static_cast<std::ptrdiff_t>(position_),
                           bytes_.begin() + static_cast<std::ptrdiff_t>(position_ + size));
        position_ += size; // ELI5: move past the text we copied.
        return result; // ELI5: return the decoded text.
    }
    void finish() const {
        if (position_ != bytes_.size()) throw ProtocolError("M3 payload has trailing bytes"); // ELI5: reject extra bytes the message format did not explain.
    }
private:
    void require(std::size_t count) const {
        if (count > bytes_.size() - position_) throw ProtocolError("M3 payload is truncated"); // ELI5: stop before trying to read beyond the message.
    }
    const Bytes& bytes_; // ELI5: keep access to the original message.
    std::size_t position_{}; // ELI5: remember where the next field begins.
};

bool has_nul(const std::string& value) { return value.find('\0') != std::string::npos; } // ELI5: detect a zero byte in a name.

std::uint32_t page_count_for(std::uint32_t total, std::uint32_t per_page) {
    return total == 0 ? 0 : static_cast<std::uint32_t>((total + per_page - 1) / per_page); // ELI5: count needed pages and round up the last partial page.
}

} // ELI5: finish the private encoding helpers.

Bytes encode_catalog_request() { return {}; } // ELI5: make an empty message because a catalog request has no extra fields.

void decode_catalog_request(const Bytes& payload) {
    if (!payload.empty()) throw ProtocolError("CATALOG_REQUEST payload must be empty"); // ELI5: reject extra bytes in a request that should contain nothing.
}

Bytes encode_file_announce(const FileManifest& manifest) {
    validate_manifest(manifest, false); // ELI5: check file facts before putting them on the network.
    Writer writer; // ELI5: prepare the message bytes.
    writer.raw(manifest.file_id.data(), manifest.file_id.size()); // ELI5: add the file's unique ID.
    writer.u64(manifest.file_size); // ELI5: add the total file size.
    writer.u32(manifest.chunk_size); // ELI5: add the normal piece size.
    writer.u32(manifest.chunk_count); // ELI5: add the total number of pieces.
    writer.u32(manifest.final_chunk_size); // ELI5: add the final piece length.
    writer.u16(static_cast<std::uint16_t>(manifest.display_name.size())); // ELI5: add the name's byte length.
    writer.raw(reinterpret_cast<const std::uint8_t*>(manifest.display_name.data()), manifest.display_name.size()); // ELI5: add the name bytes.
    return writer.take(); // ELI5: enforce the packet limit and return the message.
}

FileManifest decode_file_announce(const Bytes& payload) {
    Reader reader(payload); // ELI5: safely read each field from the network bytes.
    FileManifest manifest; // ELI5: prepare the file description to fill in.
    reader.raw(manifest.file_id.data(), manifest.file_id.size()); // ELI5: read the file ID.
    manifest.file_size = reader.u64(); // ELI5: read the total size.
    manifest.chunk_size = reader.u32(); // ELI5: read the normal piece size.
    manifest.chunk_count = reader.u32(); // ELI5: read the number of pieces.
    manifest.final_chunk_size = reader.u32(); // ELI5: read the last piece's length.
    const auto name_size = reader.u16(); // ELI5: read how many name bytes follow.
    if (name_size == 0 || name_size > max_display_name_bytes) {
        throw ProtocolError("FILE_ANNOUNCE display name has invalid length"); // ELI5: reject empty or too-long names.
    }
    manifest.display_name = reader.text(name_size); // ELI5: read the human-friendly filename.
    reader.finish(); // ELI5: reject bytes after the expected announcement fields.
    if (has_nul(manifest.display_name)) throw ProtocolError("FILE_ANNOUNCE display name contains NUL"); // ELI5: do not accept a name with a hidden zero byte.
    try {
        validate_manifest(manifest, false); // ELI5: check that size and piece-count details agree.
    } catch (const std::exception& error) {
        throw ProtocolError(std::string("invalid FILE_ANNOUNCE: ") + error.what()); // ELI5: report validation problems as protocol errors.
    }
    return manifest; // ELI5: return the checked file description.
}

std::vector<ManifestPage> make_manifest_pages(const FileManifest& manifest) {
    validate_manifest(manifest); // ELI5: require one valid checksum for every piece.
    const auto count = page_count_for(manifest.chunk_count, manifest_hashes_per_page); // ELI5: calculate how many packets the checksums need.
    std::vector<ManifestPage> pages; // ELI5: prepare the list of packets.
    pages.reserve(count); // ELI5: make enough room for them.
    for (std::uint32_t page_index = 0; page_index < count; ++page_index) {
        const auto first = page_index * manifest_hashes_per_page; // ELI5: find the first piece this page covers.
        const auto last = std::min<std::uint32_t>(manifest.chunk_count, first + manifest_hashes_per_page); // ELI5: stop at the file's end or this page's limit.
        ManifestPage page; // ELI5: create one checksum packet.
        page.file_id = manifest.file_id; // ELI5: identify which file the packet describes.
        page.page_index = page_index; // ELI5: number this packet.
        page.page_count = count; // ELI5: tell receivers how many packets complete the list.
        page.first_chunk_index = first; // ELI5: tell receivers which piece number starts here.
        page.hashes.assign(manifest.chunk_hashes.begin() + first, manifest.chunk_hashes.begin() + last); // ELI5: copy this page's checksums.
        pages.push_back(std::move(page)); // ELI5: add the packet to the return list.
    }
    return pages; // ELI5: return all checksum packets.
}

Bytes encode_manifest_page(const ManifestPage& page) {
    if (page.page_count == 0 || page.page_count > page_count_for(max_chunk_count, manifest_hashes_per_page) ||
        page.page_index >= page.page_count || page.hashes.empty() ||
        page.hashes.size() > manifest_hashes_per_page ||
        page.first_chunk_index != page.page_index * manifest_hashes_per_page) {
        throw ProtocolError("MANIFEST_PAGE fields are inconsistent"); // ELI5: reject impossible page numbers, ranges, or lengths.
    }
    Writer writer; // ELI5: prepare bytes for one checksum packet.
    writer.raw(page.file_id.data(), page.file_id.size());
    writer.u32(page.page_index);
    writer.u32(page.page_count);
    writer.u32(page.first_chunk_index);
    writer.u16(static_cast<std::uint16_t>(page.hashes.size())); // ELI5: say how many checksums follow.
    for (const auto& hash : page.hashes) writer.raw(hash.data(), hash.size()); // ELI5: add every checksum in this page.
    return writer.take(); // ELI5: enforce the message-size cap and return the bytes.
}

ManifestPage decode_manifest_page(const Bytes& payload) {
    Reader reader(payload); // ELI5: safely read one checksum packet.
    ManifestPage page; // ELI5: prepare the page's fields.
    reader.raw(page.file_id.data(), page.file_id.size());
    page.page_index = reader.u32();
    page.page_count = reader.u32();
    page.first_chunk_index = reader.u32();
    const auto hash_count = reader.u16(); // ELI5: read how many hashes are included.
    if (page.page_count == 0 || page.page_count > page_count_for(max_chunk_count, manifest_hashes_per_page) ||
        page.page_index >= page.page_count || hash_count == 0 ||
        hash_count > manifest_hashes_per_page ||
        page.first_chunk_index != page.page_index * manifest_hashes_per_page) {
        throw ProtocolError("MANIFEST_PAGE fields are outside supported bounds"); // ELI5: reject values outside the protocol's allowed range.
    }
    page.hashes.resize(hash_count); // ELI5: make one slot for each checksum.
    for (auto& hash : page.hashes) reader.raw(hash.data(), hash.size()); // ELI5: fill those slots with the received checksums.
    reader.finish(); // ELI5: ensure the packet has no trailing bytes.
    return page; // ELI5: give the checked page to the caller.
}

std::vector<HavePage> make_have_pages(const FileId& file_id,
                                      std::uint64_t generation,
                                      const std::vector<std::uint8_t>& present_by_chunk) {
    if (present_by_chunk.size() > max_chunk_count) throw ProtocolError("HAVE bitmap exceeds supported file size"); // ELI5: refuse checklists for unsupported large files.
    const auto count = page_count_for(static_cast<std::uint32_t>(present_by_chunk.size()), have_bits_per_page); // ELI5: calculate how many compact pages are needed.
    std::vector<HavePage> pages; // ELI5: prepare the page list.
    pages.reserve(count); // ELI5: make room for every page.
    for (std::uint32_t page_index = 0; page_index < count; ++page_index) {
        const auto first = page_index * have_bits_per_page; // ELI5: find the first piece number in this page.
        const auto last = std::min<std::uint32_t>(static_cast<std::uint32_t>(present_by_chunk.size()),
                                                  first + have_bits_per_page); // ELI5: stop at the file's end or this page's limit.
        HavePage page; // ELI5: create one piece-checklist packet.
        page.file_id = file_id; // ELI5: identify the file the checklist describes.
        page.generation = generation; // ELI5: label the checklist version.
        page.page_index = page_index; // ELI5: number this page.
        page.page_count = count; // ELI5: say how many pages make up the full checklist.
        page.first_chunk_index = first; // ELI5: say which piece number starts here.
        page.bit_count = static_cast<std::uint16_t>(last - first); // ELI5: count how many piece entries this page covers.
        page.bitmap.assign((page.bit_count + 7U) / 8U, 0); // ELI5: allocate enough zeroed bits to describe them.
        for (std::uint32_t i = 0; i < page.bit_count; ++i) {
            if (present_by_chunk[first + i] != 0) page.bitmap[i / 8U] |= static_cast<std::uint8_t>(1U << (i % 8U)); // ELI5: turn on the bit for a piece the peer has.
        }
        pages.push_back(std::move(page)); // ELI5: add this finished page to the result.
    }
    return pages; // ELI5: return all checklist pages.
}

Bytes encode_have_page(const HavePage& page) {
    const auto expected_bytes = (page.bit_count + 7U) / 8U; // ELI5: calculate bytes needed for this many bits.
    if (page.generation == 0 || page.page_count == 0 ||
        page.page_count > page_count_for(max_chunk_count, have_bits_per_page) ||
        page.page_index >= page.page_count || page.bit_count == 0 ||
        page.bit_count > have_bits_per_page || page.first_chunk_index != page.page_index * have_bits_per_page ||
        page.bitmap.size() != expected_bytes) {
        throw ProtocolError("HAVE_PAGE fields are inconsistent"); // ELI5: reject invalid checklist labels or sizes.
    }
    if ((page.bit_count % 8U) != 0) {
        const auto used_mask = static_cast<std::uint8_t>((1U << (page.bit_count % 8U)) - 1U); // ELI5: find which bits in the last byte actually represent pieces.
        if ((page.bitmap.back() & static_cast<std::uint8_t>(~used_mask)) != 0) {
            throw ProtocolError("HAVE_PAGE has nonzero padding bits"); // ELI5: reject values hidden in unused bits.
        }
    }
    Writer writer; // ELI5: prepare the bytes for one checklist page.
    writer.raw(page.file_id.data(), page.file_id.size());
    writer.u64(page.generation); // ELI5: add the version number.
    writer.u32(page.page_index); // ELI5: add this page's number.
    writer.u32(page.page_count); // ELI5: add the total page count.
    writer.u32(page.first_chunk_index); // ELI5: add the first piece number covered.
    writer.u16(page.bit_count); // ELI5: add the number of valid bits.
    writer.raw(page.bitmap); // ELI5: add the packed ownership bits.
    return writer.take(); // ELI5: enforce the size limit and return the packet.
}

HavePage decode_have_page(const Bytes& payload) {
    Reader reader(payload); // ELI5: safely read the checklist packet.
    HavePage page; // ELI5: prepare its fields.
    reader.raw(page.file_id.data(), page.file_id.size());
    page.generation = reader.u64(); // ELI5: read which checklist version was sent.
    page.page_index = reader.u32(); // ELI5: read the page number.
    page.page_count = reader.u32(); // ELI5: read the total pages.
    page.first_chunk_index = reader.u32(); // ELI5: read the first piece number this page covers.
    page.bit_count = reader.u16(); // ELI5: read how many ownership bits are valid.
    if (page.generation == 0 || page.page_count == 0 ||
        page.page_count > page_count_for(max_chunk_count, have_bits_per_page) ||
        page.page_index >= page.page_count || page.bit_count == 0 ||
        page.bit_count > have_bits_per_page ||
        page.first_chunk_index != page.page_index * have_bits_per_page) {
        throw ProtocolError("HAVE_PAGE fields are outside supported bounds"); // ELI5: reject impossible labels or sizes.
    }
    page.bitmap = reader.bytes((page.bit_count + 7U) / 8U); // ELI5: read the packed yes-or-no bits.
    reader.finish(); // ELI5: ensure no extra bytes follow the bitmap.
    if ((page.bit_count % 8U) != 0) {
        const auto used_mask = static_cast<std::uint8_t>((1U << (page.bit_count % 8U)) - 1U); // ELI5: identify valid positions in the last bitmap byte.
        if ((page.bitmap.back() & static_cast<std::uint8_t>(~used_mask)) != 0) {
            throw ProtocolError("HAVE_PAGE has nonzero padding bits"); // ELI5: reject set bits that do not describe a piece.
        }
    }
    return page; // ELI5: return the checked checklist page.
}

bool have_page_contains(const HavePage& page, std::uint32_t chunk_index) {
    if (chunk_index < page.first_chunk_index || chunk_index >= page.first_chunk_index + page.bit_count) return false; // ELI5: this page cannot answer about a piece outside its range.
    const auto relative = chunk_index - page.first_chunk_index; // ELI5: convert the piece number to a page-local position.
    return (page.bitmap[relative / 8U] & static_cast<std::uint8_t>(1U << (relative % 8U))) != 0; // ELI5: read and return that piece's bit.
}

} // namespace cn::filestore::wire: M3 network message encoding is implemented here.
