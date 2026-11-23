#include "filestore/manifest.hpp" // ELI5: get the manifest type and helper declarations.

#include "hashing/sha256.hpp" // ELI5: calculate file and piece checksums.

#include <filesystem> // ELI5: read the source file's name from its path.
#include <fstream> // ELI5: read the source file from disk.
#include <limits> // ELI5: detect numbers that are too large to store.
#include <stdexcept> // ELI5: explain invalid files and read failures.
#include <utility> // ELI5: move strings without making extra copies.

namespace cn::filestore { // ELI5: keep these routines in file storage.
namespace { // ELI5: keep this tiny helper private to this source file.

bool contains_nul(const std::string& text) {
    return text.find('\0') != std::string::npos; // ELI5: say whether the name contains a zero byte.
} // ELI5: finish the private name-checking helper.

} // ELI5: finish the private helper area.

FileManifest build_manifest(const std::string& path,
                            std::string display_name,
                            std::uint32_t chunk_size) {
    if (chunk_size != default_chunk_size) throw std::invalid_argument("the M3 format uses fixed 16384-byte chunks"); // ELI5: this version of M3 only accepts its agreed piece size.
    if (display_name.empty()) display_name = std::filesystem::path(path).filename().string(); // ELI5: use the source file's name when none was supplied.
    if (display_name.empty() || display_name.size() > max_display_name_bytes || contains_nul(display_name)) {
        throw std::invalid_argument("display name must be 1 to 512 bytes and contain no NUL"); // ELI5: reject empty, oversized, or unsafe names.
    }

    std::ifstream input(path, std::ios::binary); // ELI5: open the file as raw bytes so its contents are unchanged.
    if (!input) throw std::runtime_error("could not open source file: " + path); // ELI5: stop if the file cannot be read.

    FileManifest manifest; // ELI5: create the record that will describe this file.
    manifest.display_name = std::move(display_name); // ELI5: save the name peers will see.
    manifest.chunk_size = chunk_size; // ELI5: save the agreed size for normal pieces.

    hashing::Sha256 file_hash; // ELI5: start calculating the hash of the whole file.
    Bytes buffer(chunk_size); // ELI5: make a temporary buffer one piece wide.
    std::uint64_t total_size = 0; // ELI5: begin counting all bytes read.
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size())); // ELI5: try to fill one piece from the file.
        const auto read_count = input.gcount(); // ELI5: find out how many bytes actually arrived.
        if (read_count < 0) throw std::runtime_error("negative read size while chunking file"); // ELI5: treat an impossible negative count as a read failure.
        if (read_count == 0) break; // ELI5: no more bytes means the file is finished.

        const auto amount = static_cast<std::size_t>(read_count); // ELI5: convert this piece length to the container's size type.
        if (amount > std::numeric_limits<std::uint64_t>::max() - total_size) { // ELI5: check that adding these bytes will not wrap around.
            throw std::overflow_error("source file size exceeds uint64 range"); // ELI5: report a file too large for the stored size field.
        }
        total_size += static_cast<std::uint64_t>(amount); // ELI5: add this piece's bytes to the whole-file size.
        file_hash.update(buffer.data(), amount); // ELI5: add this piece to the whole-file checksum.
        manifest.chunk_hashes.push_back(hashing::sha256(Bytes(buffer.begin(), buffer.begin() + amount))); // ELI5: hash this one piece and remember its checksum.
        manifest.final_chunk_size = static_cast<std::uint32_t>(amount); // ELI5: remember this length; it will be the last-piece length after the loop.
        if (manifest.chunk_hashes.size() > max_chunk_count) { // ELI5: keep the piece list within the supported limit.
            throw std::length_error("file exceeds the configured one-million-chunk limit"); // ELI5: stop before an oversized manifest is built.
        }
    }
    if (!input.eof()) throw std::runtime_error("error while reading source file: " + path); // ELI5: report disk errors that were not a normal end of file.

    manifest.file_id = file_hash.finish(); // ELI5: use the whole-file checksum as its unique ID.
    manifest.file_size = total_size; // ELI5: save the total number of bytes read.
    manifest.chunk_count = static_cast<std::uint32_t>(manifest.chunk_hashes.size()); // ELI5: save how many pieces were made.
    if (manifest.chunk_count == 0) manifest.final_chunk_size = 0; // ELI5: an empty file has no final piece.
    validate_manifest(manifest); // ELI5: check the completed record before returning it.
    return manifest; // ELI5: give the finished file description to the caller.
}

void validate_manifest(const FileManifest& manifest, bool require_hashes) {
    if (manifest.chunk_size != default_chunk_size) throw std::invalid_argument("manifest chunk size must be 16384 bytes"); // ELI5: reject a piece size that breaks the M3 agreement.
    if (manifest.display_name.empty() || manifest.display_name.size() > max_display_name_bytes ||
        contains_nul(manifest.display_name)) {
        throw std::invalid_argument("manifest display name is invalid"); // ELI5: reject a missing, too-long, or unsafe file name.
    }
    if (manifest.chunk_count > max_chunk_count) {
        throw std::length_error("manifest exceeds the configured one-million-chunk limit"); // ELI5: reject a file description with too many pieces.
    }

    const std::uint64_t expected_count = manifest.file_size / manifest.chunk_size + // ELI5: count full-size pieces...
        (manifest.file_size % manifest.chunk_size == 0 ? 0 : 1); // ELI5: ...and add one if there are leftover bytes.
    if (expected_count != manifest.chunk_count) { // ELI5: compare the calculated count with the saved count.
        throw std::invalid_argument("manifest chunk count does not match file size"); // ELI5: reject counts that do not fit the file size.
    }
    const std::uint32_t expected_final = manifest.chunk_count == 0 // ELI5: an empty file has no last piece...
        ? 0 // ELI5: ...so its last-piece size is zero.
        : static_cast<std::uint32_t>(manifest.file_size - // ELI5: otherwise, subtract all full earlier pieces...
                                     static_cast<std::uint64_t>(manifest.chunk_size) * // ELI5: ...from the total file size...
                                         (manifest.chunk_count - 1)); // ELI5: ...to find the exact length of the last piece.
    if (manifest.final_chunk_size != expected_final ||
        (manifest.chunk_count != 0 && expected_final == 0)) {
        throw std::invalid_argument("manifest final chunk length is inconsistent"); // ELI5: reject an impossible final-piece length.
    }
    if (require_hashes && manifest.chunk_hashes.size() != manifest.chunk_count) {
        throw std::invalid_argument("manifest hash count does not match chunk count"); // ELI5: full manifests need one checksum for every piece.
    }
    if (!require_hashes && !manifest.chunk_hashes.empty() &&
        manifest.chunk_hashes.size() != manifest.chunk_count) {
        throw std::invalid_argument("partial manifest hash count is inconsistent"); // ELI5: if any checksums are present, require the complete list.
    }
}

std::uint32_t expected_chunk_size(const FileManifest& manifest, std::uint32_t index) {
    validate_manifest(manifest, false); // ELI5: make sure the file description is safe to use.
    if (index >= manifest.chunk_count) throw std::out_of_range("chunk index is outside the manifest"); // ELI5: reject a piece number past the end of the file.
    if (index + 1 == manifest.chunk_count) return manifest.final_chunk_size; // ELI5: the final piece can be shorter than the others.
    return manifest.chunk_size; // ELI5: every earlier piece uses the normal size.
}

} // namespace cn::filestore
