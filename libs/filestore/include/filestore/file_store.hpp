#pragma once // ELI5: prevent duplicate inclusion.

#include "common/storage_interfaces.hpp" // ELI5: use the shared transport and storage interfaces.
#include "filestore/chunk_store.hpp" // ELI5: use the local disk store for pieces and metadata.
#include "filestore/placement.hpp" // ELI5: compute which peers should keep each piece.

#include <cstddef> // ELI5: use standard size numbers.
#include <filesystem> // ELI5: accept a folder path for managed data.
#include <functional> // ELI5: let callers receive storage events.
#include <memory> // ELI5: own the hidden implementation safely.
#include <optional> // ELI5: represent optional file and peer values.
#include <string> // ELI5: accept names and error descriptions.
#include <vector> // ELI5: return lists of files, peers, and failures.

namespace cn::filestore { // ELI5: group the file-sharing coordinator types together.

enum class StorageEventKind { // ELI5: name the things the storage layer can report.
    manifest_received, // ELI5: another peer shared file details.
    holder_snapshot_received, // ELI5: another peer shared its piece checklist.
    chunk_verified, // ELI5: a downloaded piece passed its checksum check.
    replica_repaired, // ELI5: a missing extra copy was recreated.
    chunk_pruned, // ELI5: a piece no longer needed here was removed.
    insufficient_peers, // ELI5: there are too few peers to make all planned copies.
    replica_unavailable, // ELI5: the requested extra copy could not be obtained.
    transfer_queue_full, // ELI5: no more downloads can be queued right now.
    send_failed, // ELI5: a message could not be sent to a peer.
    malformed_message, // ELI5: a received message did not pass validation.
    assembly_expired, // ELI5: an incomplete set of pages timed out.
    storage_error // ELI5: reading or writing managed files failed.
};

struct StorageEvent { // ELI5: carry the details of one storage-layer event.
    StorageEventKind kind{}; // ELI5: say what happened.
    FileId file_id{}; // ELI5: say which file was involved.
    std::optional<std::uint32_t> chunk_index; // ELI5: optionally say which piece was involved.
    std::optional<PeerIdentity> peer; // ELI5: optionally say which peer was involved.
    std::string detail; // ELI5: give a readable explanation of the event.
};

struct SendFailure { // ELI5: explain one message that failed to send.
    PeerIdentity peer; // ELI5: identify the peer we tried to contact.
    std::uint8_t message_type{}; // ELI5: identify the message kind that failed.
    SendResult result{}; // ELI5: say why the transport rejected it.
};

struct SyncReport { // ELI5: summarize a synchronization attempt with one peer.
    std::size_t messages_queued{}; // ELI5: count messages accepted by the transport.
    std::vector<SendFailure> failures; // ELI5: keep the messages that could not be queued.
    bool complete() const noexcept { return failures.empty(); } // ELI5: say success when no message failed.
};

enum class MessageDisposition { handled, ignored, malformed }; // ELI5: report whether a received message was used, skipped, or invalid.

class FileStore final : public IChunkStore { // ELI5: coordinate file metadata, peer messages, and local piece storage.
public:
    // Event handlers should log or enqueue work only; they must not block or
    // re-enter FileStore while it is updating catalog/holder state.
    using EventHandler = std::function<void(const StorageEvent&)>; // ELI5: name the caller's function for hearing about storage events.

    FileStore(std::filesystem::path data_directory, // ELI5: choose the folder for saved data.
              PeerIdentity self, // ELI5: tell the store which peer it represents.
              ITransport& transport, // ELI5: provide a way to message other peers.
              const IPeerDirectory& peers, // ELI5: provide the current peer list.
              IChunkFetcher& fetcher, // ELI5: provide a way to download missing pieces.
              EventHandler events = {}); // ELI5: optionally provide a listener for storage events.
    ~FileStore(); // ELI5: clean up the coordinator's hidden state.

    FileStore(const FileStore&) = delete; // ELI5: prevent two stores from accidentally sharing one coordinator.
    FileStore& operator=(const FileStore&) = delete; // ELI5: prevent copying one store over another.
    FileStore(FileStore&&) noexcept; // ELI5: allow ownership of a store to move safely.
    FileStore& operator=(FileStore&&) noexcept; // ELI5: replace this store with one moved from elsewhere.

    // Indexes an input without modifying it, stores verified chunks under the
    // managed data directory, then announces metadata to active peers.
    FileManifest share_file(const std::string& source_path, std::string display_name = {}); // ELI5: index, store, and announce a file.

    // Called after an authenticated transport link is established or removed.
    // Joining peers exchange catalog summaries and verified local HAVE pages.
    SyncReport synchronize_with_peer(PeerIdentity peer); // ELI5: exchange known files and piece checklists with this peer.
    void on_peer_joined(PeerIdentity peer); // ELI5: react when a peer becomes connected.
    void on_peer_left(PeerIdentity peer); // ELI5: forget or repair state when a peer disconnects.
    MessageDisposition on_message(PeerIdentity source, // ELI5: identify who sent the message.
                                  std::uint8_t type, // ELI5: identify what kind of message arrived.
                                  const Bytes& payload); // ELI5: inspect its bytes and report how they were handled.

    // Requeues missing local placement work after a queue-full result.
    void retry_pending(); // ELI5: try again on work blocked by a full transfer queue.
    void recheck_placements(); // ELI5: compare local pieces with the current storage plan.
    // Call from the PeerApp timer hook. Drops incomplete network page assemblies
    // that have had no activity for the documented metadata TTL.
    void on_timer(); // ELI5: expire unfinished network-page collections after their timeout.

    std::vector<FileManifest> catalog() const; // ELI5: return all known file descriptions.
    PlacementPlan placement_for(const FileId& file_id) const; // ELI5: return the current keeper plan for one file.
    std::vector<PeerIdentity> confirmed_holders(const ChunkKey& key) const; // ELI5: list peers confirmed to have a piece.

    bool has_chunk(const ChunkKey& key) const override; // ELI5: answer whether this computer has a piece.
    std::error_code read_chunk(const ChunkKey& key, Bytes& output) const override; // ELI5: read a piece from this computer's storage.
    std::error_code put_verified_chunk(const ChunkKey& key, const Bytes& bytes) override; // ELI5: store bytes only after the expected checksum matches.
    std::optional<FileManifest> find_manifest(const FileId& file_id) const override; // ELI5: find a file's details if they are known.

private:
    struct Impl; // ELI5: keep the large implementation hidden in the source file.
    std::shared_ptr<Impl> impl_; // ELI5: own and share access to the hidden implementation.
};

} // namespace cn::filestore: the file-sharing coordinator interface is complete.
