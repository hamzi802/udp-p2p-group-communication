#include "filestore/chunk_store.hpp" // ELI5: declare the disk-backed storage class.

#include "filestore/manifest.hpp" // ELI5: check metadata and calculate expected piece lengths.
#include "hashing/sha256.hpp" // ELI5: verify file and piece checksums.

#include <algorithm> // ELI5: copy bytes and compare lists.
#include <atomic> // ELI5: make temporary filenames unique.
#include <array> // ELI5: hold short file-format markers.
#include <fstream> // ELI5: read and write files.
#include <limits> // ELI5: check that file sizes fit in memory.
#include <stdexcept> // ELI5: report bad files and unsupported metadata.
#include <system_error> // ELI5: return standard filesystem errors.

namespace cn::filestore { // ELI5: keep this code in the file-storage module.
namespace { // ELI5: hide low-level file helpers from other source files.

std::atomic<std::uint64_t> temporary_sequence{0}; // ELI5: give each temporary write a different number, even if calls overlap.

class BinaryWriter { // ELI5: turn numbers and text into bytes for saving on disk.
public:
    void raw(const std::uint8_t* data, std::size_t count) {
        if (count == 0) return; // ELI5: there is nothing to append when the requested size is zero.
        bytes.insert(bytes.end(), data, data + count); // ELI5: copy these bytes to the end of the output list.
    }
    void u16(std::uint16_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value >> 8)); // ELI5: write the high byte first.
        bytes.push_back(static_cast<std::uint8_t>(value)); // ELI5: then write the low byte.
    }
    void u32(std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift)); // ELI5: write four bytes from most significant to least significant.
    }
    void u64(std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift)); // ELI5: write eight bytes in a fixed network-friendly order.
    }
    Bytes bytes; // ELI5: collect all bytes the writer creates.
};

class BinaryReader { // ELI5: safely take numbers and text back out of saved bytes.
public:
    explicit BinaryReader(const Bytes& value) : data_(value) {} // ELI5: read from the given byte list without copying it.
    void raw(std::uint8_t* output, std::size_t count) {
        require(count); // ELI5: make sure enough unread bytes exist.
        std::copy_n(data_.begin() + static_cast<std::ptrdiff_t>(position), count, output); // ELI5: copy the next bytes to the caller's buffer.
        position += count; // ELI5: move the reading point past those bytes.
    }
    std::uint16_t u16() {
        require(2); // ELI5: ensure two bytes are available before reading a 16-bit number.
        auto value = static_cast<std::uint16_t>((data_[position] << 8) | data_[position + 1]);
        position += 2; // ELI5: mark those two bytes as consumed.
        return value; // ELI5: return the decoded number.
    }
    std::uint32_t u32() {
        require(4); // ELI5: make sure four bytes are available.
        std::uint32_t value = 0; // ELI5: start the decoded number at zero.
        for (int i = 0; i < 4; ++i) value = (value << 8) | data_[position++]; // ELI5: add each byte from left to right.
        return value; // ELI5: return the 32-bit result.
    }
    std::uint64_t u64() {
        require(8); // ELI5: make sure eight bytes are available.
        std::uint64_t value = 0; // ELI5: start the decoded number at zero.
        for (int i = 0; i < 8; ++i) value = (value << 8) | data_[position++]; // ELI5: add each byte from left to right.
        return value; // ELI5: return the 64-bit result.
    }
    std::string text(std::size_t count) {
        require(count); // ELI5: make sure the complete text is present.
        std::string value(data_.begin() + static_cast<std::ptrdiff_t>(position),
                          data_.begin() + static_cast<std::ptrdiff_t>(position + count));
        position += count; // ELI5: move past the text we just read.
        return value; // ELI5: give the decoded text to the caller.
    }
    Bytes bytes(std::size_t count) {
        require(count); // ELI5: make sure the requested bytes are present.
        Bytes value(data_.begin() + static_cast<std::ptrdiff_t>(position),
                    data_.begin() + static_cast<std::ptrdiff_t>(position + count));
        position += count; // ELI5: move past the returned bytes.
        return value; // ELI5: give the copied bytes to the caller.
    }
    void finish() const {
        if (position != data_.size()) throw std::runtime_error("manifest file has trailing bytes"); // ELI5: reject saved data that has unexplained extra bytes.
    }
private:
    void require(std::size_t count) const {
        if (count > data_.size() - position) throw std::runtime_error("manifest file is truncated"); // ELI5: stop before reading past the saved byte list.
    }
    const Bytes& data_; // ELI5: look at the original saved byte list.
    std::size_t position{}; // ELI5: remember the next unread byte's index.
};

Bytes encode_manifest(const FileManifest& manifest) {
    validate_manifest(manifest); // ELI5: refuse to save inconsistent file details.
    BinaryWriter writer; // ELI5: prepare a byte list for the saved format.
    writer.raw(reinterpret_cast<const std::uint8_t*>("CNMF"), 4); // ELI5: add a marker so the file format can be recognized.
    writer.u16(1); // ELI5: record format version one.
    writer.raw(manifest.file_id.data(), manifest.file_id.size()); // ELI5: write the file's unique ID.
    writer.u64(manifest.file_size); // ELI5: write the total file size.
    writer.u32(manifest.chunk_size); // ELI5: write the normal piece size.
    writer.u32(manifest.chunk_count); // ELI5: write how many pieces exist.
    writer.u32(manifest.final_chunk_size); // ELI5: write the last piece's exact size.
    writer.u16(static_cast<std::uint16_t>(manifest.display_name.size())); // ELI5: write how long the display name is.
    writer.raw(reinterpret_cast<const std::uint8_t*>(manifest.display_name.data()), manifest.display_name.size()); // ELI5: write the display name bytes.
    for (const auto& hash : manifest.chunk_hashes) writer.raw(hash.data(), hash.size()); // ELI5: write each piece checksum in order.
    return std::move(writer.bytes); // ELI5: return the complete saved representation.
}

FileManifest decode_manifest(const Bytes& bytes) {
    BinaryReader reader(bytes); // ELI5: prepare to read the saved metadata bytes safely.
    std::array<std::uint8_t, 4> magic{}; // ELI5: make room for the format marker.
    reader.raw(magic.data(), magic.size()); // ELI5: read the marker from the beginning.
    if (magic != std::array<std::uint8_t, 4>{{'C', 'N', 'M', 'F'}}) {
        throw std::runtime_error("manifest file has an unknown format"); // ELI5: reject files that are not manifest records.
    }
    if (reader.u16() != 1) throw std::runtime_error("manifest file version is unsupported"); // ELI5: reject metadata versions this code cannot understand.

    FileManifest manifest; // ELI5: prepare an empty file description.
    reader.raw(manifest.file_id.data(), manifest.file_id.size()); // ELI5: read its file ID bytes.
    manifest.file_size = reader.u64(); // ELI5: read the total file size.
    manifest.chunk_size = reader.u32(); // ELI5: read the normal piece size.
    manifest.chunk_count = reader.u32(); // ELI5: read the number of pieces.
    manifest.final_chunk_size = reader.u32(); // ELI5: read the exact size of the last piece.
    const auto name_size = reader.u16(); // ELI5: read the number of bytes in the display name.
    if (name_size == 0 || name_size > max_display_name_bytes) {
        throw std::runtime_error("manifest file display name length is invalid"); // ELI5: require a reasonable non-empty name.
    }
    manifest.display_name = reader.text(name_size); // ELI5: read the human-friendly name.
    if (manifest.chunk_count > max_chunk_count) throw std::runtime_error("manifest chunk count is too large"); // ELI5: stop before allocating an unreasonable number of hashes.
    manifest.chunk_hashes.resize(manifest.chunk_count); // ELI5: make room for one checksum per piece.
    for (auto& hash : manifest.chunk_hashes) reader.raw(hash.data(), hash.size()); // ELI5: read the checksums in file order.
    reader.finish(); // ELI5: ensure the saved file contains no extra unexplained bytes.
    validate_manifest(manifest); // ELI5: check that all decoded facts agree.
    return manifest; // ELI5: give the checked file description back.
}

Bytes read_all(const std::filesystem::path& path, std::uint64_t maximum) {
    std::error_code ec; // ELI5: reserve a place for a filesystem error.
    const auto size = std::filesystem::file_size(path, ec); // ELI5: ask the operating system how many bytes the file has.
    if (ec) throw std::filesystem::filesystem_error("could not stat file", path, ec); // ELI5: report if the size could not be read.
    if (size > maximum || size > std::numeric_limits<std::size_t>::max()) {
        throw std::length_error("file exceeds the configured read limit"); // ELI5: refuse files too large for this read or memory size.
    }
    Bytes bytes(static_cast<std::size_t>(size)); // ELI5: make exactly enough space for the file.
    std::ifstream input(path, std::ios::binary); // ELI5: open it as raw bytes.
    if (!input) throw std::runtime_error("could not open file: " + path.string()); // ELI5: report if it cannot be opened.
    if (!bytes.empty()) input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); // ELI5: read its bytes into the new list.
    if (!input || static_cast<std::size_t>(input.gcount()) != bytes.size()) {
        throw std::runtime_error("could not read complete file: " + path.string()); // ELI5: report if fewer bytes arrived than expected.
    }
    return bytes; // ELI5: return the complete file contents.
}

void write_file(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc); // ELI5: create or replace the destination file.
    if (!output) throw std::runtime_error("could not open file for writing: " + path.string()); // ELI5: report if the output cannot be opened.
    if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()),
                                     static_cast<std::streamsize>(bytes.size())); // ELI5: write all bytes to disk.
    output.flush(); // ELI5: ask the operating system to finish writing now.
    if (!output) throw std::runtime_error("could not write complete file: " + path.string()); // ELI5: report if the write did not finish.
}

void write_atomic(const std::filesystem::path& path, const Bytes& bytes) {
    std::filesystem::create_directories(path.parent_path()); // ELI5: make sure the destination folder exists.
    auto temporary = path; // ELI5: start a separate temporary filename from the final path.
    temporary += ".tmp-" + std::to_string(++temporary_sequence); // ELI5: add a unique suffix so writes do not collide.
    try {
        write_file(temporary, bytes); // ELI5: finish writing the temporary file first.
        std::error_code ec; // ELI5: reserve space for a rename error.
        std::filesystem::rename(temporary, path, ec); // ELI5: publish it under the final name once complete.
        if (ec) throw std::filesystem::filesystem_error("could not publish temporary file", temporary, path, ec); // ELI5: report if the completed file could not be installed.
    } catch (...) {
        std::error_code ignored; // ELI5: keep cleanup errors separate from the original failure.
        std::filesystem::remove(temporary, ignored); // ELI5: remove a leftover partial file.
        throw; // ELI5: let the original error reach the caller.
    }
}

std::error_code error(std::errc value) { return std::make_error_code(value); } // ELI5: turn a simple error name into a standard error_code.

} // ELI5: finish the private disk-format helpers.

ChunkStore::ChunkStore(std::filesystem::path root) {
    if (root.empty()) throw std::invalid_argument("chunk store root cannot be empty"); // ELI5: require a folder to hold managed data.
    root_ = std::filesystem::absolute(std::move(root)); // ELI5: remember it as a full path.
    std::filesystem::create_directories(root_ / "chunks"); // ELI5: create the folder for piece bytes.
    std::filesystem::create_directories(root_ / "manifests"); // ELI5: create the folder for file descriptions.
}

std::filesystem::path ChunkStore::manifest_path(const FileId& file_id) const {
    return root_ / "manifests" / (hashing::to_hex(file_id) + ".manifest"); // ELI5: use the file hash as the manifest's safe filename.
}

std::filesystem::path ChunkStore::holders_path(const FileId& file_id) const {
    return root_ / "holders" / (hashing::to_hex(file_id) + ".holders"); // ELI5: build the filename for saved peer checklists.
}

std::filesystem::path ChunkStore::chunk_directory(const FileId& file_id) const {
    return root_ / "chunks" / hashing::to_hex(file_id); // ELI5: put all pieces for a file in a folder named by its hash.
}

std::filesystem::path ChunkStore::chunk_path(const ChunkKey& key) const {
    return chunk_directory(key.file_id) / (std::to_string(key.index) + ".chunk"); // ELI5: name a piece by its number inside its file folder.
}

void ChunkStore::save_manifest(const FileManifest& manifest) {
    validate_manifest(manifest); // ELI5: check all file facts before saving them.
    const auto path = manifest_path(manifest.file_id); // ELI5: calculate the metadata filename.
    if (std::filesystem::exists(path)) { // ELI5: never silently replace a file's existing identity record.
        const auto old = decode_manifest(read_all(path, 32ULL * max_chunk_count + 1024ULL));
        if (old.file_id != manifest.file_id || old.file_size != manifest.file_size ||
            old.display_name != manifest.display_name ||
            old.chunk_size != manifest.chunk_size || old.chunk_count != manifest.chunk_count ||
            old.final_chunk_size != manifest.final_chunk_size || old.chunk_hashes != manifest.chunk_hashes) {
            throw std::runtime_error("file ID already has a different immutable manifest"); // ELI5: one file ID must always mean the same content.
        }
        return; // ELI5: the existing matching manifest is already saved.
    }
    write_atomic(path, encode_manifest(manifest)); // ELI5: encode it and safely publish it to disk.
}

FileManifest ChunkStore::load_manifest(const FileId& file_id) const {
    const auto path = manifest_path(file_id); // ELI5: find the metadata file for this ID.
    auto manifest = decode_manifest(read_all(path, 32ULL * max_chunk_count + 1024ULL)); // ELI5: read and decode it within a safe size limit.
    if (manifest.file_id != file_id) throw std::runtime_error("manifest file ID does not match its storage key"); // ELI5: ensure the filename and contents agree.
    return manifest; // ELI5: return the checked file description.
}

std::optional<FileManifest> ChunkStore::find_manifest(const FileId& file_id) const {
    if (!std::filesystem::exists(manifest_path(file_id))) return std::nullopt; // ELI5: say nothing is known when the metadata file is absent.
    return load_manifest(file_id); // ELI5: otherwise load and validate the saved description.
}

std::vector<FileManifest> ChunkStore::load_manifests() const {
    std::vector<FileManifest> result; // ELI5: prepare a list of saved file descriptions.
    const auto directory = root_ / "manifests"; // ELI5: choose the metadata folder to scan.
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".manifest") continue; // ELI5: skip folders and unrelated files.
        const auto stem = entry.path().stem().string(); // ELI5: take the file-hash text from the filename.
        const auto file_id = hashing::from_hex(stem); // ELI5: turn that text back into hash bytes.
        result.push_back(load_manifest(file_id)); // ELI5: load the matching file facts into the result.
    }
    return result; // ELI5: return every valid manifest found.
}

void ChunkStore::save_confirmed_holders(const FileId& file_id, const ConfirmedHolderTable& holders) {
    const auto manifest = load_manifest(file_id); // ELI5: get the piece count needed to check each peer bitmap.
    if (holders.size() > 1024) throw std::length_error("holder table exceeds 1024 peer sessions"); // ELI5: cap how many peer records are saved for one file.

    BinaryWriter writer; // ELI5: build the holder-list file as bytes.
    writer.raw(reinterpret_cast<const std::uint8_t*>("CNHR"), 4); // ELI5: add a marker that identifies this as holder metadata.
    writer.u16(1); // ELI5: record the holder file format version.
    writer.raw(file_id.data(), file_id.size()); // ELI5: record which file these peer lists describe.
    writer.u32(static_cast<std::uint32_t>(manifest.chunk_count)); // ELI5: record how many pieces each list must cover.
    writer.u16(static_cast<std::uint16_t>(holders.size())); // ELI5: record the number of peer lists that follow.
    const auto bitmap_size = (manifest.chunk_count + 7U) / 8U; // ELI5: calculate bytes needed for one bit per piece.
    for (const auto& [peer, state] : holders) {
        if (state.present_by_chunk.size() != manifest.chunk_count) { // ELI5: every peer list must have one entry per piece.
            throw std::invalid_argument("holder bitmap length does not match manifest"); // ELI5: reject a checklist with the wrong length.
        }
        writer.u32(peer.id);
        writer.u64(peer.session);
        writer.u64(state.generation);
        writer.u32(bitmap_size);
        Bytes bitmap(bitmap_size, 0); // ELI5: make a compact bit list, starting with every piece marked absent.
        for (std::uint32_t i = 0; i < manifest.chunk_count; ++i) {
            if (state.present_by_chunk[i] != 0) bitmap[i / 8U] |= static_cast<std::uint8_t>(1U << (i % 8U)); // ELI5: set the piece's bit when this peer has it.
        }
        writer.raw(bitmap.data(), bitmap.size()); // ELI5: append the compact checklist bytes.
    }
    write_atomic(holders_path(file_id), writer.bytes); // ELI5: safely save the complete peer table.
}

ConfirmedHolderTable ChunkStore::load_confirmed_holders(const FileId& file_id) const {
    const auto path = holders_path(file_id); // ELI5: find the peer-table filename.
    if (!std::filesystem::exists(path)) return {}; // ELI5: no saved peer table means no known remote holders.
    const auto manifest = load_manifest(file_id); // ELI5: use the file description to validate the table.
    const auto maximum = 44ULL + 1024ULL * (24ULL + (manifest.chunk_count + 7ULL) / 8ULL);
    const auto file_bytes = read_all(path, maximum); // ELI5: read it with a maximum size based on supported peers and pieces.
    BinaryReader reader(file_bytes); // ELI5: prepare to decode it.
    std::array<std::uint8_t, 4> magic{};
    reader.raw(magic.data(), magic.size()); // ELI5: read its format marker.
    if (magic != std::array<std::uint8_t, 4>{{'C', 'N', 'H', 'R'}} || reader.u16() != 1) {
        throw std::runtime_error("holder metadata file has an unknown format"); // ELI5: reject data that is not a supported holder table.
    }
    FileId recorded_file{}; // ELI5: reserve space for the file ID stored inside the table.
    reader.raw(recorded_file.data(), recorded_file.size()); // ELI5: read that ID.
    if (recorded_file != file_id || reader.u32() != manifest.chunk_count) {
        throw std::runtime_error("holder metadata does not match its manifest"); // ELI5: ensure these peer records belong to this file.
    }
    const auto peer_count = reader.u16(); // ELI5: find out how many peer records to decode.
    if (peer_count > 1024) throw std::runtime_error("holder metadata peer count is too large"); // ELI5: reject an unsupported number of peers.
    const auto expected_bitmap_size = (manifest.chunk_count + 7U) / 8U; // ELI5: calculate the expected compact list size.
    ConfirmedHolderTable result; // ELI5: prepare the table to return.
    for (std::uint16_t i = 0; i < peer_count; ++i) {
        PeerIdentity peer; // ELI5: prepare to read one peer's identity.
        peer.id = reader.u32(); // ELI5: read the peer number.
        peer.session = reader.u64(); // ELI5: read the peer's current session number.
        ConfirmedHolderState state; // ELI5: prepare this peer's saved checklist.
        state.generation = reader.u64(); // ELI5: read which checklist version this is.
        if (reader.u32() != expected_bitmap_size) throw std::runtime_error("holder bitmap size is invalid"); // ELI5: require enough bits for every file piece.
        const auto bitmap = reader.bytes(expected_bitmap_size); // ELI5: read the packed yes-or-no bits.
        state.present_by_chunk.assign(manifest.chunk_count, 0); // ELI5: create one unpacked entry for every piece.
        for (std::uint32_t index = 0; index < manifest.chunk_count; ++index) {
            state.present_by_chunk[index] = (bitmap[index / 8U] & static_cast<std::uint8_t>(1U << (index % 8U))) ? 1 : 0; // ELI5: turn each packed bit into a simple 1 or 0 entry.
        }
        if ((manifest.chunk_count % 8U) != 0 && !bitmap.empty()) {
            const auto used_mask = static_cast<std::uint8_t>((1U << (manifest.chunk_count % 8U)) - 1U); // ELI5: mark which bits in the final byte are real piece entries.
            if ((bitmap.back() & static_cast<std::uint8_t>(~used_mask)) != 0) { // ELI5: check that unused bits at the end are clear.
                throw std::runtime_error("holder bitmap has nonzero padding bits"); // ELI5: reject hidden data in unused bit positions.
            }
        }
        if (!result.emplace(peer, std::move(state)).second) { // ELI5: add this identity unless it was already present.
            throw std::runtime_error("holder metadata repeats a peer session"); // ELI5: reject duplicate peer entries.
        }
    }
    reader.finish(); // ELI5: reject unexpected extra bytes after the peer records.
    return result; // ELI5: return the checked peer checklist table.
}

void ChunkStore::store_file(const std::string& source_path, const FileManifest& manifest) {
    validate_manifest(manifest); // ELI5: verify the file description before touching its pieces.

    const auto final_directory = chunk_directory(manifest.file_id); // ELI5: calculate where finished pieces belong.
    const auto staging = root_ / "chunks" /
        (".staging-" + hashing::to_hex(manifest.file_id) + "-" + std::to_string(++temporary_sequence));
    std::filesystem::create_directories(staging); // ELI5: write pieces to a temporary folder before publishing them.
    try {
        std::ifstream input(source_path, std::ios::binary); // ELI5: reopen the source file as raw bytes.
        if (!input) throw std::runtime_error("could not open source file: " + source_path); // ELI5: report if it disappeared or cannot be read.
        hashing::Sha256 full_hash; // ELI5: independently recheck the whole-file checksum.
        std::uint64_t total_size = 0; // ELI5: count bytes while copying pieces.
        for (std::uint32_t index = 0; index < manifest.chunk_count; ++index) {
            const auto expected_size = expected_chunk_size(manifest, index); // ELI5: find the correct size for this piece.
            Bytes chunk(expected_size); // ELI5: make space for one piece.
            input.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(expected_size)); // ELI5: read that many bytes from the source.
            if (input.gcount() != static_cast<std::streamsize>(expected_size)) {
                throw std::runtime_error("source file changed or ended while storing chunks"); // ELI5: stop if the source no longer matches the manifest's expected length.
            }
            full_hash.update(chunk); // ELI5: include this piece in the whole-file checksum.
            total_size += chunk.size(); // ELI5: add this piece's length to the total.
            if (hashing::sha256(chunk) != manifest.chunk_hashes[index]) {
                throw std::runtime_error("source chunk changed after manifest creation"); // ELI5: do not save a piece whose content changed since indexing.
            }
            write_file(staging / (std::to_string(index) + ".chunk"), chunk); // ELI5: save this checked piece in the temporary folder.
        }
        char extra{}; // ELI5: reserve one extra byte to check for file growth.
        if (input.read(&extra, 1)) throw std::runtime_error("source file grew after manifest creation"); // ELI5: detect extra source bytes beyond the manifest size.
        if (!input.eof()) throw std::runtime_error("error while reading source file during storage"); // ELI5: distinguish a normal end from a disk read error.
        if (total_size != manifest.file_size || full_hash.finish() != manifest.file_id) {
            throw std::runtime_error("source file changed after manifest creation"); // ELI5: require both total length and whole-file ID to still match.
        }

        if (!std::filesystem::exists(final_directory)) { // ELI5: use the complete temporary folder if no pieces were stored before.
            std::filesystem::rename(staging, final_directory); // ELI5: publish all verified pieces together.
        } else {
            for (std::uint32_t index = 0; index < manifest.chunk_count; ++index) {
                const auto staged = staging / (std::to_string(index) + ".chunk"); // ELI5: name this newly checked temporary piece.
                const auto target = chunk_path(ChunkKey{manifest.file_id, index}); // ELI5: find the final location for this piece.
                std::filesystem::create_directories(target.parent_path()); // ELI5: make sure its file folder exists.
                const auto bytes = read_all(staged, default_chunk_size); // ELI5: load the verified temporary bytes.
                bool existing_is_valid = false; // ELI5: assume an old saved copy needs repair until checked.
                if (std::filesystem::is_regular_file(target)) { // ELI5: if an older copy exists, verify it before replacing.
                    try {
                        const auto existing = read_all(target, default_chunk_size); // ELI5: read the old piece from disk.
                        existing_is_valid = existing.size() == expected_chunk_size(manifest, index) && // ELI5: check its expected length...
                                            hashing::sha256(existing) == manifest.chunk_hashes[index]; // ELI5: ...and its checksum.
                    } catch (const std::exception&) {
                        existing_is_valid = false; // ELI5: unreadable old data is not a valid copy.
                    }
                }
                if (!existing_is_valid) {
                    write_atomic(target, bytes); // ELI5: replace the bad or missing piece safely.
                }
            }
            std::filesystem::remove_all(staging); // ELI5: remove temporary pieces after updating final copies.
        }
        save_manifest(manifest); // ELI5: persist the description after the pieces are safely placed.
    } catch (...) {
        std::error_code ignored; // ELI5: do not replace the original error with a cleanup error.
        std::filesystem::remove_all(staging, ignored); // ELI5: remove any unfinished temporary pieces.
        throw; // ELI5: let the original problem reach the caller.
    }
}

bool ChunkStore::has_chunk(const ChunkKey& key) const {
    return std::filesystem::is_regular_file(chunk_path(key)); // ELI5: answer whether the piece's path is a regular file.
}

std::error_code ChunkStore::read_chunk(const ChunkKey& key, Bytes& output) const {
    try {
        const auto manifest = load_manifest(key.file_id); // ELI5: get the expected size and checksum for this file.
        if (key.index >= manifest.chunk_count) return error(std::errc::invalid_argument); // ELI5: reject a piece number outside the file.
        const auto path = chunk_path(key); // ELI5: find that piece's filename.
        if (!std::filesystem::is_regular_file(path)) return error(std::errc::no_such_file_or_directory); // ELI5: report when no saved piece exists.
        auto bytes = read_all(path, default_chunk_size); // ELI5: read the saved piece, with a maximum-size check.
        if (bytes.size() != expected_chunk_size(manifest, key.index) ||
            hashing::sha256(bytes) != manifest.chunk_hashes[key.index]) {
            return error(std::errc::illegal_byte_sequence); // ELI5: report bytes that fail the expected size or checksum.
        }
        output = std::move(bytes); // ELI5: give the verified bytes to the caller.
        return {}; // ELI5: an empty error means the read succeeded.
    } catch (const std::filesystem::filesystem_error& exception) {
        return exception.code(); // ELI5: pass through the operating system's file error.
    } catch (const std::exception&) {
        return error(std::errc::io_error); // ELI5: turn other read or metadata problems into a standard I/O error.
    }
}

std::error_code ChunkStore::put_verified_chunk(const ChunkKey& key, const Bytes& bytes) {
    try {
        const auto manifest = load_manifest(key.file_id); // ELI5: load the expected piece size and hash.
        if (key.index >= manifest.chunk_count) return error(std::errc::invalid_argument); // ELI5: reject a piece number past the end.
        if (bytes.size() != expected_chunk_size(manifest, key.index)) return error(std::errc::invalid_argument); // ELI5: reject a piece with the wrong byte length.
        if (hashing::sha256(bytes) != manifest.chunk_hashes[key.index]) {
            return error(std::errc::illegal_byte_sequence); // ELI5: reject bytes that do not match the expected checksum.
        }
        write_atomic(chunk_path(key), bytes); // ELI5: safely save only verified piece bytes.
        return {}; // ELI5: report success with an empty error.
    } catch (const std::filesystem::filesystem_error& exception) {
        return exception.code(); // ELI5: pass through filesystem-specific problems.
    } catch (const std::exception&) {
        return error(std::errc::io_error); // ELI5: report other storage problems as I/O errors.
    }
}

std::vector<std::uint8_t> ChunkStore::local_presence(const FileId& file_id) const {
    const auto manifest = load_manifest(file_id); // ELI5: learn how many pieces this file has.
    std::vector<std::uint8_t> present(manifest.chunk_count, 0); // ELI5: start by marking every piece absent.
    for (std::uint32_t index = 0; index < manifest.chunk_count; ++index) {
        Bytes ignored; // ELI5: hold each piece temporarily just to verify it.
        present[index] = read_chunk(ChunkKey{file_id, index}, ignored) ? 0 : 1; // ELI5: mark the piece present only if it can be read and verified.
    }
    return present; // ELI5: return the verified yes-or-no list.
}

std::error_code ChunkStore::erase_chunk(const ChunkKey& key) {
    std::error_code ec; // ELI5: make room to receive a filesystem problem.
    const bool removed = std::filesystem::remove(chunk_path(key), ec); // ELI5: try to remove the selected piece file.
    if (ec) return ec; // ELI5: return the operating system's error if removal failed.
    return removed ? std::error_code{} : error(std::errc::no_such_file_or_directory); // ELI5: say success if removed, or not-found if it was already gone.
}

} // namespace cn::filestore: verified file pieces and metadata are stored here.
