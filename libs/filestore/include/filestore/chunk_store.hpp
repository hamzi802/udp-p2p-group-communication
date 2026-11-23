#pragma once // ELI5: prevent duplicate inclusion.

#include "common/storage_interfaces.hpp" // ELI5: implement the shared chunk-storage promise.

#include <filesystem> // ELI5: build and inspect paths on disk.
#include <map> // ELI5: map each peer identity to its saved checklist.
#include <vector> // ELI5: keep lists of pieces and file records.

namespace cn::filestore { // ELI5: group local file-storage types together.

struct ConfirmedHolderState { // ELI5: remember one peer's latest confirmed piece checklist.
    std::uint64_t generation{}; // ELI5: identify how new this checklist is.
    std::vector<std::uint8_t> present_by_chunk; // ELI5: store one yes-or-no entry for each piece.
};
using ConfirmedHolderTable = std::map<PeerIdentity, ConfirmedHolderState>; // ELI5: look up each peer's checklist by identity.

class ChunkStore final : public cn::IChunkStore { // ELI5: keep checked file pieces and metadata on disk.
public:
    explicit ChunkStore(std::filesystem::path root); // ELI5: choose the folder where managed data lives.

    bool has_chunk(const ChunkKey& key) const override; // ELI5: check whether a piece file exists.
    std::error_code read_chunk(const ChunkKey& key, Bytes& output) const override; // ELI5: load a piece's bytes from disk.
    std::error_code put_verified_chunk(const ChunkKey& key, const Bytes& bytes) override; // ELI5: save a piece that has already passed its hash check.
    std::optional<FileManifest> find_manifest(const FileId& file_id) const override; // ELI5: load file details if this store knows them.

    void save_manifest(const FileManifest& manifest); // ELI5: save a file's description and checksums.
    std::vector<FileManifest> load_manifests() const; // ELI5: read all saved file descriptions.
    void save_confirmed_holders(const FileId& file_id, const ConfirmedHolderTable& holders); // ELI5: persist which peers were last confirmed to hold pieces.
    ConfirmedHolderTable load_confirmed_holders(const FileId& file_id) const; // ELI5: reload the saved peer checklists.
    void store_file(const std::string& source_path, const FileManifest& manifest); // ELI5: split a source file and save its pieces.
    std::vector<std::uint8_t> local_presence(const FileId& file_id) const; // ELI5: report which pieces this computer has.
    std::error_code erase_chunk(const ChunkKey& key); // ELI5: remove one saved piece and report any disk error.

    const std::filesystem::path& root() const noexcept { return root_; } // ELI5: let callers see the chosen storage folder.

private:
    std::filesystem::path manifest_path(const FileId& file_id) const; // ELI5: build the metadata filename for a file ID.
    std::filesystem::path holders_path(const FileId& file_id) const; // ELI5: build the saved peer-checklist filename.
    std::filesystem::path chunk_directory(const FileId& file_id) const; // ELI5: build the folder path for a file's pieces.
    std::filesystem::path chunk_path(const ChunkKey& key) const; // ELI5: build the exact filename for one piece.
    FileManifest load_manifest(const FileId& file_id) const; // ELI5: load one file's metadata, failing if it is missing or invalid.

    std::filesystem::path root_; // ELI5: remember the top folder for all managed data.
};

} // namespace cn::filestore: the disk-backed chunk store is declared.
