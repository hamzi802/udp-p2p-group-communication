#pragma once // ELI5: prevent duplicate inclusion.

#include "common/file_types.hpp" // ELI5: reuse the common file and hash types.

#include <cstddef> // ELI5: use standard size numbers.
#include <cstdint> // ELI5: use fixed-width numbers for saved metadata.
#include <string> // ELI5: store a human-readable file name.

namespace cn::filestore { // ELI5: group file-storage helpers together.

inline constexpr std::uint32_t default_chunk_size = 16U * 1024U; // ELI5: split files into 16 KiB pieces by default.
inline constexpr std::uint32_t max_chunk_count = 1'000'000U; // ELI5: refuse manifests with an unreasonable number of pieces.
inline constexpr std::size_t max_display_name_bytes = 512U; // ELI5: cap names so metadata stays small and safe.

// ELI5: read a file once to calculate its whole-file ID and every piece checksum.
FileManifest build_manifest(const std::string& path,
                            std::string display_name = {}, // ELI5: optionally choose the name other peers see.
                            std::uint32_t chunk_size = default_chunk_size); // ELI5: optionally choose the piece size.

void validate_manifest(const FileManifest& manifest, bool require_hashes = true); // ELI5: reject file details that are inconsistent or unsafe.
std::uint32_t expected_chunk_size(const FileManifest& manifest, std::uint32_t index); // ELI5: calculate the correct byte count for one piece.

} // namespace cn::filestore: the manifest helpers are declared.
