#include "filestore/file_store.hpp" // ELI5: get the coordinator's class and public function declarations.

#include "filestore/manifest.hpp" // ELI5: validate file descriptions and calculate piece sizes.
#include "filestore/wire.hpp" // ELI5: encode and decode the M3 network messages.
#include "hashing/sha256.hpp" // ELI5: verify file data using checksums.

#include <algorithm> // ELI5: sort, search, and filter peer or piece lists.
#include <chrono> // ELI5: measure when temporary page assemblies expire.
#include <limits> // ELI5: detect counters that would overflow.
#include <map> // ELI5: look up records by file, peer, or page number.
#include <set> // ELI5: keep unique pending work and reported events.
#include <stdexcept> // ELI5: report invalid inputs and protocol problems.
#include <utility> // ELI5: move large objects instead of copying them.

namespace cn::filestore { // ELI5: keep the coordinator implementation in its module.
namespace { // ELI5: keep helper names private to this source file.

using AssemblyKey = std::pair<PeerIdentity, FileId>; // ELI5: identify a page bundle by sender and file.
constexpr std::size_t max_manifest_assemblies = 64; // ELI5: limit how many unfinished manifests can wait.
constexpr std::size_t max_pending_manifest_hash_bytes = 64U * 1024U * 1024U; // ELI5: cap memory used by waiting file checksums at 64 MiB.
constexpr std::size_t max_have_assemblies = 1024; // ELI5: limit how many unfinished piece checklists can wait.
constexpr std::size_t max_pending_have_bytes = 16U * 1024U * 1024U; // ELI5: cap memory used by waiting checklists at 16 MiB.
constexpr auto incomplete_page_ttl = std::chrono::seconds(30); // ELI5: discard a page bundle after 30 quiet seconds.
constexpr auto failed_source_retry_delay = std::chrono::seconds(5); // ELI5: wait five seconds before retrying a peer that failed.

std::uint32_t pages_for(std::uint32_t total, std::uint32_t page_size) {
    return total == 0 ? 0 : (total + page_size - 1) / page_size; // ELI5: divide into pages and round up when some items remain.
}

bool same_header(const FileManifest& lhs, const FileManifest& rhs) {
    return lhs.file_id == rhs.file_id && lhs.display_name == rhs.display_name && // ELI5: compare identity and the visible name...
           lhs.file_size == rhs.file_size && lhs.chunk_size == rhs.chunk_size &&
           lhs.chunk_count == rhs.chunk_count && lhs.final_chunk_size == rhs.final_chunk_size; // ELI5: ...and all the other fixed file-size facts.
}

bool valid_peer(PeerIdentity peer) { return peer.id != 0 && peer.session != 0; } // ELI5: peer number and session must both be nonzero.

std::filesystem::path absolute_data_directory(std::filesystem::path path) {
    if (path.empty()) throw std::invalid_argument("FileStore data directory cannot be empty"); // ELI5: require somewhere to save managed data.
    return std::filesystem::absolute(std::move(path)); // ELI5: turn the chosen folder into a full path.
}

} // ELI5: finish the file-private helpers.

struct FileStore::Impl : std::enable_shared_from_this<FileStore::Impl> { // ELI5: hide working state and allow safe async callbacks to refer back to it.
    struct FileRecord { // ELI5: keep one file's facts and peer checklists together.
        FileManifest manifest; // ELI5: store the verified description of the whole file.
        ConfirmedHolderTable holders; // ELI5: store which active peers say they own each piece.
        std::optional<std::size_t> reported_degraded_peer_count; // ELI5: remember if a low-peer warning was already sent.
    };

    struct ManifestAssembly { // ELI5: collect pieces of one manifest arriving in separate messages.
        bool has_header{}; // ELI5: say whether the basic file details arrived.
        FileManifest header; // ELI5: hold those basic file details.
        std::uint32_t page_count{}; // ELI5: remember how many checksum pages are expected.
        std::map<std::uint32_t, wire::ManifestPage> pages; // ELI5: save each received page by its number.
        std::size_t retained_hash_bytes{}; // ELI5: count memory used by this partial manifest.
        std::chrono::steady_clock::time_point touched{std::chrono::steady_clock::now()}; // ELI5: remember when a message last updated it.
    };

    struct HaveAssembly { // ELI5: collect a peer's piece checklist from separate messages.
        std::uint64_t generation{}; // ELI5: remember which checklist version these pages belong to.
        std::uint32_t page_count{}; // ELI5: remember how many checklist pages are expected.
        std::map<std::uint32_t, wire::HavePage> pages; // ELI5: save each received page by its number.
        std::size_t retained_bytes{}; // ELI5: count the memory taken by this partial checklist.
        std::chrono::steady_clock::time_point touched{std::chrono::steady_clock::now()}; // ELI5: remember when a message last updated it.
    };

    std::filesystem::path data_directory; // ELI5: remember the full folder for managed files.
    PeerIdentity self; // ELI5: remember this peer's identity.
    ITransport* transport{}; // ELI5: borrow the object that sends messages.
    const IPeerDirectory* peers{}; // ELI5: borrow the object that lists connected peers.
    IChunkFetcher* fetcher{}; // ELI5: borrow the object that downloads missing pieces.
    EventHandler events; // ELI5: keep the optional event listener.
    ChunkStore chunks; // ELI5: manage pieces and metadata on disk.
    std::map<FileId, FileRecord> files; // ELI5: keep the known files indexed by their IDs.
    std::map<AssemblyKey, ManifestAssembly> manifest_assemblies; // ELI5: hold incomplete manifest downloads.
    std::map<AssemblyKey, HaveAssembly> have_assemblies; // ELI5: hold incomplete checklist downloads.
    std::set<ChunkKey> pending_fetches; // ELI5: avoid requesting the same piece twice at once.
    std::map<ChunkKey, std::map<PeerIdentity, std::chrono::steady_clock::time_point>> failed_sources; // ELI5: remember which peers recently failed for each piece.
    std::set<ChunkKey> reported_unavailable; // ELI5: avoid repeating the same unavailable-piece warning.
    std::set<ChunkKey> reported_queue_full; // ELI5: avoid repeating the same full-queue warning.
    std::size_t pending_manifest_hash_bytes{}; // ELI5: track memory used by incomplete manifests.
    std::size_t pending_have_bytes{}; // ELI5: track memory used by incomplete checklists.

    Impl(std::filesystem::path directory,
         PeerIdentity identity,
         ITransport& transport_ref,
         const IPeerDirectory& peer_directory,
         IChunkFetcher& chunk_fetcher,
         EventHandler event_handler)
        : data_directory(absolute_data_directory(std::move(directory))), // ELI5: store the data folder as a full path.
          self(identity), // ELI5: remember this peer's identity.
          transport(&transport_ref), // ELI5: remember which object sends network messages.
          peers(&peer_directory), // ELI5: remember which object lists active peers.
          fetcher(&chunk_fetcher), // ELI5: remember which object downloads missing pieces.
          events(std::move(event_handler)), // ELI5: keep the optional event callback.
          chunks(data_directory / "chunk_store") { // ELI5: place this module's chunk data in a child folder.
        if (!valid_peer(self)) throw std::invalid_argument("FileStore requires nonzero local ID and session"); // ELI5: reject an unusable local identity.
        for (auto& manifest : chunks.load_manifests()) {
            FileRecord record; // ELI5: rebuild one in-memory record from saved metadata.
            record.manifest = std::move(manifest); // ELI5: keep the saved file description.
            record.holders = chunks.load_confirmed_holders(record.manifest.file_id); // ELI5: reload all saved peer checklists.
            const auto local = chunks.local_presence(record.manifest.file_id); // ELI5: inspect which pieces still exist on disk.
            auto current = record.holders.find(self); // ELI5: look for this peer's saved checklist.
            if (current == record.holders.end() || current->second.present_by_chunk != local) {
                if (current != record.holders.end() &&
                    current->second.generation == std::numeric_limits<std::uint64_t>::max()) {
                    throw std::overflow_error("local HAVE generation exhausted; restart with a new session"); // ELI5: a new session is needed before the counter can safely restart.
                }
                const auto generation = current == record.holders.end() ? 1 : current->second.generation + 1; // ELI5: make a new checklist version.
                record.holders[self] = ConfirmedHolderState{generation, local}; // ELI5: save the corrected local checklist in memory.
                chunks.save_confirmed_holders(record.manifest.file_id, record.holders); // ELI5: persist that corrected checklist.
            }
            files.emplace(record.manifest.file_id, std::move(record)); // ELI5: add the loaded file to the catalog.
        }
    }

    void emit(StorageEventKind kind,
              const FileId& file_id,
              std::optional<std::uint32_t> chunk_index = {},
              std::optional<PeerIdentity> peer = {},
              std::string detail = {}) const {
        if (!events) return; // ELI5: there is no listener, so there is nothing to report.
        try {
            events(StorageEvent{kind, file_id, chunk_index, peer, std::move(detail)}); // ELI5: tell the listener what happened.
        } catch (...) {
            // ELI5: ignore listener errors so logging cannot stop file storage.
        }
    }

    std::vector<PeerIdentity> active_peers() const {
        auto active = peers->active_peers(); // ELI5: start with the network's current peer list.
        active.erase(std::remove_if(active.begin(), active.end(), [](PeerIdentity peer) {
            return !valid_peer(peer); // ELI5: discard entries with an empty peer number or session.
        }), active.end()); // ELI5: remove every invalid entry from the list.
        active.push_back(self); // ELI5: include this computer in the placement group.
        std::sort(active.begin(), active.end(), [](PeerIdentity lhs, PeerIdentity rhs) {
            return lhs.id < rhs.id || (lhs.id == rhs.id && lhs.session < rhs.session); // ELI5: sort by peer number, then session.
        }); // ELI5: keep peer ordering predictable.
        std::vector<PeerIdentity> unique; // ELI5: prepare a list with one entry per peer number.
        for (const auto peer : active) {
            if (!unique.empty() && unique.back().id == peer.id) {
                if (unique.back() != peer) throw std::logic_error("membership exposes two sessions for one peer ID"); // ELI5: stop if one peer number has two live sessions.
                continue; // ELI5: skip an exact duplicate identity.
            }
            unique.push_back(peer); // ELI5: keep this new peer identity.
        }
        const auto local = std::lower_bound(unique.begin(), unique.end(), self, // ELI5: find where this peer belongs in the sorted list.
            [](PeerIdentity lhs, PeerIdentity rhs) { return lhs.id < rhs.id; }); // ELI5: compare peers using their numbers.
        if (local != unique.end() && local->id == self.id) {
            if (*local != self) throw std::logic_error("local peer ID conflicts with an active remote session"); // ELI5: reject another session pretending to be this peer.
        } else {
            unique.insert(local, self); // ELI5: add this peer if the list did not already contain it.
        }
        return unique; // ELI5: give back the cleaned and sorted list.
    }

    bool is_active(PeerIdentity peer) const {
        if (peer == self) return true; // ELI5: this computer is always part of its own group.
        const auto active = peers->active_peers(); // ELI5: read the current remote peer list.
        return std::find(active.begin(), active.end(), peer) != active.end(); // ELI5: report whether this exact peer is connected.
    }

    SyncReport send_message(PeerIdentity peer, std::uint8_t type, const Bytes& payload) {
        SyncReport report; // ELI5: start with an empty send summary.
        const auto result = transport->send_reliable(peer, type, payload); // ELI5: ask the transport to queue this message.
        if (result == SendResult::ok) {
            report.messages_queued = 1; // ELI5: count the message because the transport accepted it.
        } else {
            report.failures.push_back(SendFailure{peer, type, result}); // ELI5: remember which message could not be queued.
            emit(StorageEventKind::send_failed, FileId{}, {}, peer, // ELI5: report the failed send to the event listener.
                 "reliable M3 message was not queued; repeat synchronization after the transport recovers"); // ELI5: explain that the caller can retry synchronization later.
        }
        return report; // ELI5: return whether this message was queued.
    }

    static void merge(SyncReport& into, SyncReport from) {
        into.messages_queued += from.messages_queued; // ELI5: add the new successful sends to the total.
        into.failures.insert(into.failures.end(), from.failures.begin(), from.failures.end()); // ELI5: append the new send failures.
    }

    void send_local_have(FileRecord& record, PeerIdentity peer, SyncReport& report) {
        const auto state = record.holders.find(self); // ELI5: look up this peer's own piece checklist.
        if (state == record.holders.end()) return; // ELI5: nothing can be announced until a checklist exists.
        for (const auto& page : wire::make_have_pages(record.manifest.file_id,
                                                       state->second.generation,
                                                       state->second.present_by_chunk)) {
            merge(report, send_message(peer, message_type::have_page, wire::encode_have_page(page))); // ELI5: send each checklist page and record whether it queued.
        }
    }

    void send_file(PeerIdentity peer, FileRecord& record, SyncReport& report) {
        merge(report, send_message(peer, message_type::file_announce, // ELI5: send the basic facts about this file first.
                                   wire::encode_file_announce(record.manifest))); // ELI5: turn those facts into network bytes.
        for (const auto& page : wire::make_manifest_pages(record.manifest)) {
            merge(report, send_message(peer, message_type::manifest_page, // ELI5: send each page of piece checksums.
                                       wire::encode_manifest_page(page))); // ELI5: turn the checksum page into network bytes.
        }
        send_local_have(record, peer, report);
    }

    SyncReport send_catalog(PeerIdentity peer) {
        SyncReport report; // ELI5: start an empty report for this file catalog.
        for (auto& [file_id, record] : files) {
            (void)file_id; // ELI5: this loop needs the record, not the map's file-ID key.
            send_file(peer, record, report); // ELI5: announce this known file and its local pieces.
        }
        return report; // ELI5: return the combined send results.
    }

    SyncReport sync_peer(PeerIdentity peer) {
        if (!valid_peer(peer) || peer == self) throw std::invalid_argument("cannot synchronize with invalid/self peer"); // ELI5: only a real remote peer can be synchronized.
        SyncReport report; // ELI5: start an empty summary.
        merge(report, send_message(peer, message_type::catalog_request, wire::encode_catalog_request())); // ELI5: ask the peer to send files it knows about.
        merge(report, send_catalog(peer)); // ELI5: also tell it about every file we know.
        return report; // ELI5: return all send outcomes to the caller.
    }

    bool update_local_bitmap(FileRecord& record, std::vector<std::uint8_t> bitmap) {
        auto found = record.holders.find(self);
        if (found != record.holders.end() && found->second.present_by_chunk == bitmap) return false;
        std::uint64_t generation = 1;
        if (found != record.holders.end()) {
            if (found->second.generation == std::numeric_limits<std::uint64_t>::max()) {
                throw std::overflow_error("local HAVE generation exhausted; restart with a new session");
            }
            generation = found->second.generation + 1;
        }
        record.holders[self] = ConfirmedHolderState{generation, std::move(bitmap)};
        chunks.save_confirmed_holders(record.manifest.file_id, record.holders);
        return true;
    }

    void broadcast_local_have(FileRecord& record) {
        const auto active = peers->active_peers();
        for (const auto peer : active) {
            if (!valid_peer(peer) || peer == self) continue;
            SyncReport report;
            send_local_have(record, peer, report);
        }
    }

    std::vector<PeerIdentity> holders_for(const FileRecord& record, std::uint32_t index) const {
        std::vector<PeerIdentity> result;
        for (const auto& [peer, state] : record.holders) {
            if (index < state.present_by_chunk.size() && state.present_by_chunk[index] != 0 && is_active(peer)) {
                result.push_back(peer);
            }
        }
        std::sort(result.begin(), result.end(), [](PeerIdentity lhs, PeerIdentity rhs) {
            return lhs.id < rhs.id;
        });
        return result;
    }

    std::vector<PeerIdentity> available_sources(const ChunkKey& key, const FileRecord& record) const {
        auto sources = holders_for(record, key.index); // ELI5: begin with peers confirmed to have this piece.
        const auto failed = failed_sources.find(key); // ELI5: check whether some peers recently failed for it.
        const auto now = std::chrono::steady_clock::now(); // ELI5: read the current time for retry checks.
        sources.erase(std::remove_if(sources.begin(), sources.end(), [&](PeerIdentity peer) {
            if (peer == self) return true; // ELI5: do not download from our own disk through the network.
            if (failed == failed_sources.end()) return false; // ELI5: no failure record means this source is ready to try.
            const auto retry_after = failed->second.find(peer); // ELI5: look up this peer's wait-until time.
            return retry_after != failed->second.end() && now < retry_after->second; // ELI5: temporarily skip peers whose retry time has not arrived.
        }), sources.end()); // ELI5: remove peers that should not be tried yet.
        return sources; // ELI5: give back the usable download sources.
    }

    void set_unavailable(const ChunkKey& key, std::string detail) {
        if (!reported_unavailable.insert(key).second) return; // ELI5: report this missing piece only once until it is repaired.
        emit(StorageEventKind::replica_unavailable, key.file_id, key.index, {}, std::move(detail)); // ELI5: tell the listener that no source is currently available.
    }

    void fetch_complete(const ChunkKey& key, PeerIdentity source, ChunkFetchResult result) {
        pending_fetches.erase(key); // ELI5: this download request is no longer running.
        reported_queue_full.erase(key); // ELI5: a later queue-full warning may now be useful.
        const auto record_it = files.find(key.file_id); // ELI5: find the file this piece belongs to.
        if (record_it == files.end()) return; // ELI5: ignore the result if the file was removed meanwhile.
        auto& record = record_it->second; // ELI5: use the file's saved state.
        if (result.error) {
            failed_sources[key][source] = std::chrono::steady_clock::now() + failed_source_retry_delay; // ELI5: pause before asking this peer again.
            emit(StorageEventKind::storage_error, key.file_id, key.index, source,
                 "chunk fetch failed: " + result.error.message()); // ELI5: report the download problem in readable form.
            schedule_chunk(key, record); // ELI5: look for another source or decide the piece is unavailable.
            return; // ELI5: do not try to store missing bytes.
        }

        const auto store_result = chunks.put_verified_chunk(key, result.bytes); // ELI5: check the size and hash, then store the bytes.
        if (store_result) {
            failed_sources[key][source] = std::chrono::steady_clock::now() + failed_source_retry_delay; // ELI5: pause before trusting this source with the piece again.
            emit(StorageEventKind::storage_error, key.file_id, key.index, source,
                 "fetched chunk failed local size/hash verification: " + store_result.message()); // ELI5: explain that downloaded data did not pass its checks.
            schedule_chunk(key, record); // ELI5: try another confirmed holder if possible.
            return; // ELI5: never announce a piece that failed verification.
        }

        failed_sources.erase(key); // ELI5: clear the old source failures after success.
        reported_unavailable.erase(key); // ELI5: clear the unavailable warning now that the piece is repaired.
        auto local = record.holders.find(self); // ELI5: find this peer's existing piece checklist.
        auto bitmap = local == record.holders.end()
            ? std::vector<std::uint8_t>(record.manifest.chunk_count, 0) // ELI5: start with an all-missing checklist if none exists.
            : local->second.present_by_chunk; // ELI5: otherwise copy the current checklist.
        bitmap[key.index] = 1; // ELI5: mark the new piece as present.
        update_local_bitmap(record, std::move(bitmap)); // ELI5: save the changed checklist and its new version.
        emit(StorageEventKind::chunk_verified, key.file_id, key.index, source,
             "chunk verified and atomically stored"); // ELI5: report that this piece passed its checks.
        emit(StorageEventKind::replica_repaired, key.file_id, key.index, self,
             "local placement or replica count restored from a confirmed holder"); // ELI5: report that an extra planned copy was restored.
        broadcast_local_have(record); // ELI5: tell peers this computer now has the piece.
        reconcile_record(record); // ELI5: see if any other piece needs repair or cleanup.
    }

    void schedule_chunk(const ChunkKey& key, FileRecord& record) {
        if (pending_fetches.count(key) != 0) return; // ELI5: avoid starting a second request for the same piece.
        const auto local = record.holders.find(self); // ELI5: check this peer's current checklist.
        if (local != record.holders.end() && key.index < local->second.present_by_chunk.size() &&
            local->second.present_by_chunk[key.index] != 0 && chunks.has_chunk(key)) return; // ELI5: do nothing if our saved piece is already present.
        const auto sources = available_sources(key, record); // ELI5: list active peers that can currently supply it.
        if (sources.empty()) {
            set_unavailable(key, "no active confirmed holder can supply this chunk"); // ELI5: report that the piece has no usable source.
            return; // ELI5: there is nobody to ask right now.
        }
        const auto expected_size = expected_chunk_size(record.manifest, key.index); // ELI5: calculate how many bytes this piece should contain.
        const auto weak = weak_from_this(); // ELI5: make a safe reference for the later download callback.
        for (const auto source : sources) {
            pending_fetches.insert(key); // ELI5: mark this piece as being requested.
            const bool queued = fetcher->fetch_chunk(source, key, expected_size,
                [weak, key, source](ChunkFetchResult result) mutable {
                    if (auto self_state = weak.lock()) self_state->fetch_complete(key, source, std::move(result)); // ELI5: if the store still exists, handle the finished download.
                }); // ELI5: ask the transfer module to download asynchronously.
            if (queued) return; // ELI5: stop because one request is now in progress.
            pending_fetches.erase(key); // ELI5: the request did not enter the queue, so clear its pending mark.
            if (reported_queue_full.insert(key).second) {
                emit(StorageEventKind::transfer_queue_full, key.file_id, key.index, source,
                     "M4 transfer queue is full; the next timer or retry_pending call will retry"); // ELI5: explain that a later retry will try again.
            }
            return; // ELI5: leave retrying to the timer or explicit retry call.
        }
    }

    bool holder_confirmed(const FileRecord& record, PeerIdentity peer, std::uint32_t index) const {
        const auto found = record.holders.find(peer); // ELI5: look up this peer's saved checklist.
        return found != record.holders.end() && index < found->second.present_by_chunk.size() &&
               found->second.present_by_chunk[index] != 0 && is_active(peer); // ELI5: confirm the peer is active and its checklist marks this piece present.
    }

    void reconcile_record(FileRecord& record) {
        bool removed_stale_holders = false; // ELI5: track if disconnected peers were removed.
        for (auto it = record.holders.begin(); it != record.holders.end();) {
            if (it->first != self && !is_active(it->first)) { // ELI5: remote peers that left cannot be counted as keepers.
                it = record.holders.erase(it); // ELI5: remove this disconnected peer's checklist.
                removed_stale_holders = true; // ELI5: remember that saved state changed.
            } else ++it; // ELI5: keep the local peer or an active remote peer.
        }
        if (removed_stale_holders) persist_record(record); // ELI5: save the cleaned peer checklists if needed.

        auto plan = make_placement_plan(record.manifest, active_peers()); // ELI5: calculate where pieces should live among current peers.
        const bool degraded = record.manifest.chunk_count != 0 &&
                              (!plan.has_two_distinct_holders || !plan.avoids_a_complete_managed_copy);
        if (degraded) { // ELI5: fewer than enough peers or pieces means the ideal plan cannot be met.
            if (!record.reported_degraded_peer_count ||
                *record.reported_degraded_peer_count != plan.ordered_peers.size()) {
                const auto detail = plan.ordered_peers.size() < 2 // ELI5: explain which peer-count rule is not met.
                    ? "two distinct live holders require at least two active peers"
                    : "two peers can hold two replicas, but both necessarily hold the complete managed file";
                emit(StorageEventKind::insufficient_peers, record.manifest.file_id, {}, {}, detail); // ELI5: tell the caller why redundancy is reduced.
                record.reported_degraded_peer_count = plan.ordered_peers.size(); // ELI5: remember the peer count so we do not repeat the warning.
            }
        } else {
            record.reported_degraded_peer_count.reset(); // ELI5: clear the warning state after the plan improves.
        }

        auto local = record.holders.find(self); // ELI5: find this computer's saved piece checklist.
        std::vector<std::uint8_t> local_bits = local == record.holders.end()
            ? std::vector<std::uint8_t>(record.manifest.chunk_count, 0) // ELI5: use an all-empty checklist if this is a new file.
            : local->second.present_by_chunk; // ELI5: otherwise start from the saved local checklist.
        bool local_changed = false; // ELI5: track whether pruning removes any pieces.

        for (const auto& placement : plan.chunks) {
            const ChunkKey key{record.manifest.file_id, placement.chunk_index}; // ELI5: name the file piece being checked.
            const bool self_desired = std::find(placement.holders.begin(), placement.holders.end(), self) !=
                                      placement.holders.end(); // ELI5: check whether this peer is assigned to keep it.
            const bool has_local = placement.chunk_index < local_bits.size() &&
                                   local_bits[placement.chunk_index] != 0 && chunks.has_chunk(key); // ELI5: confirm both the checklist and actual disk file say the piece exists.
            if (self_desired && !has_local) {
                schedule_chunk(key, record); // ELI5: download a piece this peer is supposed to keep.
                continue; // ELI5: skip pruning checks for this piece.
            }
            if (self_desired || !has_local || placement.holders.empty()) continue; // ELI5: only consider removing a piece we have but are not assigned to keep.

            const bool replacements_confirmed = std::all_of(placement.holders.begin(), placement.holders.end(), // ELI5: check every peer assigned to keep a replacement copy...
                [&](PeerIdentity holder) { return holder_confirmed(record, holder, placement.chunk_index); }); // ELI5: ...and require each one's checklist to confirm the piece.
            if (!replacements_confirmed) continue; // ELI5: keep our copy until all replacement copies are confirmed.
            const auto result = chunks.erase_chunk(key); // ELI5: remove our no-longer-needed copy from disk.
            if (!result || result == std::make_error_code(std::errc::no_such_file_or_directory)) {
                local_bits[placement.chunk_index] = 0; // ELI5: mark the removed piece as absent here.
                local_changed = true; // ELI5: remember that the local checklist needs saving.
                emit(StorageEventKind::chunk_pruned, key.file_id, key.index, self,
                     "removed a non-target managed replica after replacement holders confirmed storage"); // ELI5: tell the caller that safe cleanup occurred.
            }
        }

        if (local_changed) {
            update_local_bitmap(record, std::move(local_bits)); // ELI5: save the new checklist version.
            broadcast_local_have(record); // ELI5: tell the other peers that the piece was removed here.
        }
    }

    void reconcile_all() {
        for (auto& [file_id, record] : files) {
            (void)file_id; // ELI5: this loop needs each record, not its map key.
            reconcile_record(record); // ELI5: repair and clean up the current file's piece assignments.
        }
    }

    void persist_record(FileRecord& record) {
        chunks.save_confirmed_holders(record.manifest.file_id, record.holders); // ELI5: write the peer checklists to disk.
    }

    void try_finish_manifest(const AssemblyKey& key) {
        auto assembly_it = manifest_assemblies.find(key); // ELI5: find the partial manifest for this sender and file.
        if (assembly_it == manifest_assemblies.end() || !assembly_it->second.has_header) return; // ELI5: wait until its basic file details are available.
        auto& assembly = assembly_it->second; // ELI5: work with the partial pages we have collected.
        const auto expected_pages = pages_for(assembly.header.chunk_count, wire::manifest_hashes_per_page); // ELI5: calculate how many checksum pages this file needs.
        if (expected_pages == 0 && assembly.page_count != 0) {
            throw wire::ProtocolError("empty manifest must not have hash pages"); // ELI5: an empty file has no piece checksums.
        }
        if (expected_pages != 0 && assembly.page_count == 0) return; // ELI5: wait for the first checksum page to tell us the page count.
        if (assembly.page_count != expected_pages) throw wire::ProtocolError("manifest pages do not match announced chunk count"); // ELI5: reject a page count that disagrees with file size.
        if (assembly.pages.size() != expected_pages) return; // ELI5: wait until every checksum page has arrived.

        FileManifest manifest = assembly.header; // ELI5: start the finished record with the announced file facts.
        manifest.chunk_hashes.clear(); // ELI5: replace any old hash list with the received pages.
        manifest.chunk_hashes.reserve(manifest.chunk_count); // ELI5: make room for one checksum per piece.
        for (std::uint32_t index = 0; index < expected_pages; ++index) {
            const auto page_it = assembly.pages.find(index); // ELI5: get the next page in order.
            if (page_it == assembly.pages.end()) return; // ELI5: wait if any page number is missing.
            const auto expected_first = index * wire::manifest_hashes_per_page; // ELI5: calculate which piece number this page should start with.
            const auto expected_count = std::min<std::uint32_t>(wire::manifest_hashes_per_page,
                                                                 manifest.chunk_count - expected_first); // ELI5: calculate how many hashes this page should carry.
            if (page_it->second.page_count != expected_pages ||
                page_it->second.first_chunk_index != expected_first ||
                page_it->second.hashes.size() != expected_count) { // ELI5: check the page's number, starting piece, and length.
                throw wire::ProtocolError("manifest page sequence is incomplete or inconsistent"); // ELI5: reject pages that cannot form one complete ordered list.
            }
            manifest.chunk_hashes.insert(manifest.chunk_hashes.end(),
                                         page_it->second.hashes.begin(), page_it->second.hashes.end()); // ELI5: append these hashes to the full checksum list.
        }
        validate_manifest(manifest); // ELI5: make sure the assembled file description is internally consistent.

        auto found = files.find(manifest.file_id); // ELI5: check whether this file is already in the catalog.
        if (found != files.end()) {
            if (!same_header(found->second.manifest, manifest) ||
                found->second.manifest.chunk_hashes != manifest.chunk_hashes) { // ELI5: compare every fixed fact and checksum with the saved version.
                throw wire::ProtocolError("received manifest conflicts with the immutable catalog entry"); // ELI5: never let a peer change an already-known file ID.
            }
        } else {
            chunks.save_manifest(manifest); // ELI5: persist this new file's description.
            FileRecord record; // ELI5: make its in-memory tracking record.
            record.manifest = manifest; // ELI5: store the verified description in the record.
            record.holders = chunks.load_confirmed_holders(manifest.file_id); // ELI5: reload any saved peer checklists.
            const auto local_bits = chunks.local_presence(manifest.file_id); // ELI5: check which pieces this computer already has.
            auto local = record.holders.find(self); // ELI5: find this computer's checklist, if one exists.
            if (local == record.holders.end() || local->second.present_by_chunk != local_bits) {
                if (local != record.holders.end() &&
                    local->second.generation == std::numeric_limits<std::uint64_t>::max()) {
                    throw std::overflow_error("local HAVE generation exhausted; restart with a new session"); // ELI5: stop if this checklist version cannot be increased safely.
                }
                const auto generation = local == record.holders.end() ? 1 : local->second.generation + 1; // ELI5: choose the first version or increase the old version.
                record.holders[self] = ConfirmedHolderState{generation, local_bits}; // ELI5: record this computer's actual pieces.
            }
            chunks.save_confirmed_holders(manifest.file_id, record.holders); // ELI5: save the updated peer checklists on disk.
            found = files.emplace(manifest.file_id, std::move(record)).first; // ELI5: add the new file record to the catalog.
        }

        pending_manifest_hash_bytes -= assembly.retained_hash_bytes; // ELI5: release the memory budget used by this finished bundle.
        manifest_assemblies.erase(assembly_it); // ELI5: remove the now-complete temporary page collection.
        emit(StorageEventKind::manifest_received, manifest.file_id, {}, key.first,
             "complete immutable manifest stored in the local catalog"); // ELI5: tell the application the file is now known.

        std::vector<AssemblyKey> pending_haves; // ELI5: collect checklists that arrived before this file was known.
        for (const auto& [have_key, have] : have_assemblies) {
            (void)have; // ELI5: this loop only needs each checklist's key.
            if (have_key.second == manifest.file_id) pending_haves.push_back(have_key); // ELI5: remember checklists belonging to this file.
        }
        for (const auto& have_key : pending_haves) try_finish_have(have_key); // ELI5: finish any checklist that can now be checked against this file.
        reconcile_record(found->second); // ELI5: schedule repairs or cleanup for the new file.
    }

    void accept_announce(PeerIdentity source, FileManifest header) {
        const auto existing = files.find(header.file_id); // ELI5: see whether this file is already known.
        if (existing != files.end()) {
            if (!same_header(existing->second.manifest, header)) {
                throw wire::ProtocolError("FILE_ANNOUNCE conflicts with an existing file ID"); // ELI5: reject different file facts using the same ID.
            }
            SyncReport report; // ELI5: collect any message-send result.
            send_local_have(existing->second, source, report); // ELI5: tell this peer which pieces we have.
            return; // ELI5: the known file needs no new manifest assembly.
        }
        const AssemblyKey key{source, header.file_id}; // ELI5: identify this partial file record by its sender and ID.
        auto assembly = manifest_assemblies.find(key); // ELI5: look for an existing partial record.
        if (assembly == manifest_assemblies.end()) {
            if (manifest_assemblies.size() >= max_manifest_assemblies) {
                throw wire::ProtocolError("too many incomplete manifests"); // ELI5: cap memory and work from unfinished announcements.
            }
            assembly = manifest_assemblies.emplace(key, ManifestAssembly{}).first; // ELI5: start collecting a new manifest.
        }
        auto& state = assembly->second;
        state.touched = std::chrono::steady_clock::now(); // ELI5: refresh its timeout clock.
        if (state.has_header && !same_header(state.header, header)) {
            throw wire::ProtocolError("peer sent conflicting FILE_ANNOUNCE messages"); // ELI5: one sender must not change file facts mid-transfer.
        }
        state.header = std::move(header); // ELI5: keep the newly received basic file facts.
        state.has_header = true; // ELI5: mark the announcement as received.
        if (state.page_count == 0 && !state.pages.empty()) {
            throw wire::ProtocolError("manifest page count is missing"); // ELI5: pages cannot exist without declaring how many there are.
        }
        try_finish_manifest(key); // ELI5: complete the file record if all its checksum pages already arrived.
    }

    void accept_manifest_page(PeerIdentity source, wire::ManifestPage page) {
        const auto known = files.find(page.file_id); // ELI5: check whether the whole file manifest is already saved.
        if (known != files.end()) {
            const auto& manifest = known->second.manifest; // ELI5: use the saved facts as the correct answer.
            const auto expected_pages = pages_for(manifest.chunk_count, wire::manifest_hashes_per_page); // ELI5: calculate how many checksum pages should exist.
            if (page.page_count != expected_pages || page.page_index >= expected_pages) {
                throw wire::ProtocolError("manifest page does not match the known manifest"); // ELI5: reject an out-of-range or wrongly sized page set.
            }
            const auto first = page.page_index * wire::manifest_hashes_per_page; // ELI5: calculate the first piece number in this page.
            const auto expected_count = std::min<std::uint32_t>(wire::manifest_hashes_per_page,
                                                                 manifest.chunk_count - first); // ELI5: calculate how many hashes should be on it.
            if (page.first_chunk_index != first || page.hashes.size() != expected_count ||
                !std::equal(page.hashes.begin(), page.hashes.end(), manifest.chunk_hashes.begin() + first)) {
                throw wire::ProtocolError("manifest page conflicts with the known immutable manifest"); // ELI5: reject any page that changes a saved checksum.
            }
            return; // ELI5: the matching page is already known, so no assembly is needed.
        }
        const AssemblyKey key{source, page.file_id}; // ELI5: identify the partial manifest by sender and file ID.
        auto assembly = manifest_assemblies.find(key); // ELI5: look for pages we already collected.
        if (assembly == manifest_assemblies.end()) {
            if (manifest_assemblies.size() >= max_manifest_assemblies) {
                throw wire::ProtocolError("too many incomplete manifests"); // ELI5: stop if too many unfinished files are using memory.
            }
            assembly = manifest_assemblies.emplace(key, ManifestAssembly{}).first; // ELI5: start a new collection of pages.
        }
        auto& state = assembly->second;
        state.touched = std::chrono::steady_clock::now(); // ELI5: update the inactivity timeout.
        if (state.page_count != 0 && state.page_count != page.page_count) {
            throw wire::ProtocolError("manifest page count changed during transfer"); // ELI5: every page in one file must agree on the page total.
        }
        state.page_count = page.page_count; // ELI5: remember how many pages this file should have.
        const auto expected_hashes = page.page_index + 1 == page.page_count // ELI5: the last page may be shorter...
            ? page.hashes.size() : wire::manifest_hashes_per_page; // ELI5: ...while every earlier page must be full.
        if (page.hashes.size() != expected_hashes) {
            throw wire::ProtocolError("non-final manifest page is not full"); // ELI5: reject a short page in the middle of the list.
        }
        auto existing = state.pages.find(page.page_index); // ELI5: check whether this page number arrived before.
        if (existing != state.pages.end()) {
            if (existing->second.hashes != page.hashes) throw wire::ProtocolError("conflicting duplicate manifest page"); // ELI5: duplicate pages must contain the same checksums.
            try_finish_manifest(key); // ELI5: maybe the remaining pages arrived since the previous attempt.
            return; // ELI5: avoid storing the same page twice.
        }
        const auto bytes = page.hashes.size() * sizeof(Digest256); // ELI5: work out how much memory these checksums will use.
        if (bytes > max_pending_manifest_hash_bytes - pending_manifest_hash_bytes) {
            throw wire::ProtocolError("pending manifest memory budget exceeded"); // ELI5: refuse pages that would exceed the memory cap.
        }
        state.retained_hash_bytes += bytes; // ELI5: track memory used by this file's partial pages.
        pending_manifest_hash_bytes += bytes; // ELI5: track memory used across all partial manifests.
        state.pages.emplace(page.page_index, std::move(page)); // ELI5: save this checksum page.
        try_finish_manifest(key); // ELI5: assemble the full manifest if every page is now present.
    }

    void accept_have_page(PeerIdentity source, wire::HavePage page) {
        const AssemblyKey key{source, page.file_id}; // ELI5: identify this peer's checklist assembly.
        const auto file = files.find(page.file_id); // ELI5: check whether we already know this file's piece count.
        if (file != files.end()) {
            const auto current = file->second.holders.find(source); // ELI5: find the sender's newest saved checklist.
            if (current != file->second.holders.end() && current->second.generation > page.generation) return; // ELI5: ignore an older checklist that arrived late.
            if (current != file->second.holders.end() && current->second.generation == page.generation) {
                const auto expected_pages = pages_for(file->second.manifest.chunk_count, wire::have_bits_per_page); // ELI5: calculate the right checklist page count.
                if (page.page_count != expected_pages || page.page_index >= expected_pages) {
                    throw wire::ProtocolError("duplicate HAVE page does not match the stored bitmap"); // ELI5: reject page numbers that do not fit the known file.
                }
                const auto first = page.page_index * wire::have_bits_per_page; // ELI5: find the first piece number covered by this page.
                const auto expected_bits = std::min<std::uint32_t>(wire::have_bits_per_page,
                    file->second.manifest.chunk_count - first); // ELI5: count how many pieces should be listed here.
                if (page.first_chunk_index != first || page.bit_count != expected_bits ||
                    first + expected_bits > current->second.present_by_chunk.size()) {
                    throw wire::ProtocolError("duplicate HAVE page has inconsistent bounds"); // ELI5: reject an incorrectly positioned or sized page.
                }
                for (std::uint32_t bit = 0; bit < expected_bits; ++bit) {
                    const auto absolute = page.first_chunk_index + bit; // ELI5: convert this page-local bit into a file piece number.
                    const bool advertised = wire::have_page_contains(page, absolute); // ELI5: read what the repeated page claims about that piece.
                    if (advertised != (current->second.present_by_chunk[absolute] != 0)) {
                        throw wire::ProtocolError("HAVE bitmap changed without a new generation"); // ELI5: require peers to use a new version number for changed checklists.
                    }
                }
                return; // ELI5: the duplicate matches, so no new checklist data is needed.
            }
        }

        auto assembly = have_assemblies.find(key); // ELI5: look for an unfinished checklist from this sender.
        if (assembly == have_assemblies.end() || assembly->second.generation < page.generation) {
            const auto max_pages = pages_for(max_chunk_count, wire::have_bits_per_page); // ELI5: calculate the largest allowed checklist page count.
            if (page.page_count > max_pages) throw wire::ProtocolError("too many HAVE pages"); // ELI5: reject a checklist larger than supported files.
            if (assembly == have_assemblies.end() && have_assemblies.size() >= max_have_assemblies) {
                throw wire::ProtocolError("too many incomplete HAVE snapshots"); // ELI5: cap unfinished checklists to protect memory.
            }
            if (assembly != have_assemblies.end()) pending_have_bytes -= assembly->second.retained_bytes; // ELI5: free memory for an older checklist being replaced.
            HaveAssembly state; // ELI5: start an empty checklist for this newer version.
            state.generation = page.generation; // ELI5: remember its version number.
            state.page_count = page.page_count; // ELI5: remember how many pages to collect.
            assembly = have_assemblies.insert_or_assign(key, std::move(state)).first; // ELI5: save the new partial checklist.
        }
        if (assembly->second.generation > page.generation) return; // ELI5: ignore a page from an older checklist version.
        if (assembly->second.page_count != page.page_count) throw wire::ProtocolError("HAVE page count changed during transfer"); // ELI5: every page must agree on the total.
        assembly->second.touched = std::chrono::steady_clock::now(); // ELI5: refresh this checklist's timeout.
        auto slot = assembly->second.pages.find(page.page_index); // ELI5: see whether this page number was already received.
        if (slot != assembly->second.pages.end()) {
            if (slot->second.bitmap != page.bitmap || slot->second.bit_count != page.bit_count ||
                slot->second.first_chunk_index != page.first_chunk_index) {
                throw wire::ProtocolError("conflicting duplicate HAVE page"); // ELI5: duplicate pages must carry identical bits and bounds.
            }
            return; // ELI5: matching duplicate data does not need to be saved again.
        }
        if (page.bitmap.size() > max_pending_have_bytes - pending_have_bytes) {
            throw wire::ProtocolError("pending HAVE memory budget exceeded"); // ELI5: refuse data that would use too much temporary memory.
        }
        assembly->second.retained_bytes += page.bitmap.size(); // ELI5: count the bytes for this peer's partial checklist.
        pending_have_bytes += page.bitmap.size(); // ELI5: count bytes across all unfinished checklists.
        assembly->second.pages.emplace(page.page_index, std::move(page)); // ELI5: save this page in its numbered slot.
        try_finish_have(key); // ELI5: build the complete checklist if no pages are missing.
    }

    void try_finish_have(const AssemblyKey& key) {
        auto assembly = have_assemblies.find(key); // ELI5: find the partial checklist pages for this peer and file.
        auto record = files.find(key.second); // ELI5: find the file description that tells us how many pieces exist.
        if (assembly == have_assemblies.end() || record == files.end()) return; // ELI5: wait until both the pages and file description are available.
        auto& state = assembly->second; // ELI5: work with the saved partial checklist.
        const auto expected_pages = pages_for(record->second.manifest.chunk_count, wire::have_bits_per_page); // ELI5: calculate how many checklist pages the file needs.
        if (state.page_count != expected_pages) {
            throw wire::ProtocolError("HAVE pages do not match manifest chunk count"); // ELI5: reject a checklist whose page count does not fit the file.
        }
        if (state.pages.size() != expected_pages) return; // ELI5: keep waiting until every checklist page arrives.

        ConfirmedHolderState holder; // ELI5: create the complete checklist for this peer.
        holder.generation = state.generation; // ELI5: keep the checklist's version number.
        holder.present_by_chunk.assign(record->second.manifest.chunk_count, 0); // ELI5: begin with every piece marked missing.
        for (std::uint32_t page_index = 0; page_index < expected_pages; ++page_index) {
            const auto found_page = state.pages.find(page_index); // ELI5: get the next page in order.
            if (found_page == state.pages.end()) return; // ELI5: do not finish if any numbered page is missing.
            const auto& page = found_page->second; // ELI5: read this page's bitmap and labels.
            const auto first = page_index * wire::have_bits_per_page; // ELI5: calculate the first piece number on this page.
            const auto bits = std::min<std::uint32_t>(wire::have_bits_per_page,
                                                       record->second.manifest.chunk_count - first); // ELI5: calculate how many piece bits this page should contain.
            if (page.page_index != page_index || page.page_count != expected_pages ||
                page.first_chunk_index != first || page.bit_count != bits) {
                throw wire::ProtocolError("HAVE page sequence is incomplete or inconsistent"); // ELI5: reject pages with the wrong order, count, or range.
            }
            for (std::uint32_t bit = 0; bit < bits; ++bit) {
                const auto absolute = first + bit; // ELI5: turn this page position into a file piece number.
                holder.present_by_chunk[absolute] = wire::have_page_contains(page, absolute) ? 1 : 0; // ELI5: copy the peer's yes-or-no answer into the full checklist.
            }
        }

        auto& holders = record->second.holders; // ELI5: open the file's saved peer-checklist table.
        const auto old = holders.find(key.first); // ELI5: find this sender's previous checklist.
        if (old != holders.end() && old->second.generation == holder.generation &&
            old->second.present_by_chunk != holder.present_by_chunk) { // ELI5: compare same-version checklists for changes.
            throw wire::ProtocolError("HAVE bitmap changed without a new generation"); // ELI5: changed data must use a higher version number.
        }
        holders[key.first] = std::move(holder); // ELI5: replace this peer's checklist with the complete new one.
        chunks.save_confirmed_holders(key.second, holders); // ELI5: save all peer checklists for this file on disk.
        for (std::uint32_t i = 0; i < record->second.manifest.chunk_count; ++i) {
            if (holders[key.first].present_by_chunk[i] != 0) failed_sources.erase(ChunkKey{key.second, i}); // ELI5: let this peer be tried again for pieces it now confirms.
        }
        pending_have_bytes -= state.retained_bytes; // ELI5: free memory counted for the finished checklist.
        have_assemblies.erase(assembly); // ELI5: remove the temporary pages now that they are combined.
        emit(StorageEventKind::holder_snapshot_received, key.second, {}, key.first,
             "complete verified-holder bitmap recorded"); // ELI5: report that this peer's piece list is ready.
        reconcile_record(record->second); // ELI5: use the new peer information to repair or prune pieces.
    }

    void handle_message(PeerIdentity source, std::uint8_t type, const Bytes& payload) {
        switch (type) { // ELI5: choose the right decoder based on the message kind.
        case message_type::catalog_request:
            wire::decode_catalog_request(payload); // ELI5: check that this request contains no malformed data.
            (void)send_catalog(source); // ELI5: answer by sending our known files back.
            return; // ELI5: the catalog request is handled.
        case message_type::file_announce:
            accept_announce(source, wire::decode_file_announce(payload)); // ELI5: read file facts and start or update manifest assembly.
            return; // ELI5: the announcement is handled.
        case message_type::manifest_page:
            accept_manifest_page(source, wire::decode_manifest_page(payload)); // ELI5: read a checksum page and add it to the manifest.
            return; // ELI5: the checksum page is handled.
        case message_type::have_page:
            accept_have_page(source, wire::decode_have_page(payload)); // ELI5: read a peer's piece-checklist page.
            return; // ELI5: the checklist page is handled.
        default:
            throw wire::ProtocolError("unknown M3 message type"); // ELI5: reject message kinds this storage module does not recognize.
        }
    }

    void update_single_local_chunk(FileRecord& record, std::uint32_t index, bool present) {
        auto found = record.holders.find(self); // ELI5: find this computer's existing checklist.
        auto bitmap = found == record.holders.end()
            ? std::vector<std::uint8_t>(record.manifest.chunk_count, 0) // ELI5: begin with every piece absent if no checklist exists.
            : found->second.present_by_chunk; // ELI5: otherwise copy the current local checklist.
        if (index >= bitmap.size()) throw std::out_of_range("chunk index is outside the manifest"); // ELI5: reject a piece number past the file's end.
        const auto value = static_cast<std::uint8_t>(present ? 1 : 0); // ELI5: store true as 1 and false as 0.
        if (bitmap[index] == value) return; // ELI5: nothing changed, so no update needs sending.
        bitmap[index] = value; // ELI5: change this piece's yes-or-no entry.
        update_local_bitmap(record, std::move(bitmap)); // ELI5: save the new checklist with a fresh version.
        broadcast_local_have(record); // ELI5: tell connected peers about the change.
        reconcile_record(record); // ELI5: recalculate what pieces should be downloaded or removed.
    }

    FileManifest share(const std::string& source_path, std::string display_name) {
        auto manifest = build_manifest(source_path, std::move(display_name)); // ELI5: calculate the file ID and all piece checksums.
        chunks.store_file(source_path, manifest); // ELI5: split the source and save the pieces assigned locally.
        auto record_it = files.find(manifest.file_id); // ELI5: see whether this exact file is already in memory.
        if (record_it != files.end()) {
            if (!same_header(record_it->second.manifest, manifest) ||
                record_it->second.manifest.chunk_hashes != manifest.chunk_hashes) {
                throw std::runtime_error("file ID conflicts with a different stored manifest"); // ELI5: stop if identical IDs somehow describe different file data.
            }
        } else {
            FileRecord record; // ELI5: make a catalog record for this new file.
            record.manifest = manifest; // ELI5: keep its full description.
            record.holders = chunks.load_confirmed_holders(manifest.file_id); // ELI5: reload any known peer checklists.
            record_it = files.emplace(manifest.file_id, std::move(record)).first; // ELI5: add the file to the in-memory catalog.
        }

        auto& record = record_it->second; // ELI5: work with this file's catalog record.
        update_local_bitmap(record, chunks.local_presence(manifest.file_id)); // ELI5: record exactly which pieces this computer stored.
        SyncReport report; // ELI5: collect send results while notifying peers.
        for (const auto peer : peers->active_peers()) {
            if (valid_peer(peer) && peer != self) send_file(peer, record, report); // ELI5: tell every valid remote peer about the file and local pieces.
        }
        reconcile_record(record); // ELI5: make sure all required piece copies are planned.
        return manifest; // ELI5: give the file's description back to the caller.
    }

    void peer_left(PeerIdentity peer) {
        for (auto& [file_id, record] : files) { // ELI5: check every known file when a peer disconnects.
            (void)file_id; // ELI5: only the record is needed here.
            if (record.holders.erase(peer) != 0) persist_record(record); // ELI5: remove and save the peer's checklist if it was present.
        }
        for (auto it = manifest_assemblies.begin(); it != manifest_assemblies.end();) {
            if (it->first.first == peer) { // ELI5: find unfinished manifests sent by this peer.
                pending_manifest_hash_bytes -= it->second.retained_hash_bytes; // ELI5: release the memory counted for its pages.
                it = manifest_assemblies.erase(it); // ELI5: discard its incomplete manifest pages.
            } else ++it; // ELI5: leave other peers' assemblies alone.
        }
        for (auto it = have_assemblies.begin(); it != have_assemblies.end();) {
            if (it->first.first == peer) { // ELI5: find unfinished checklists sent by this peer.
                pending_have_bytes -= it->second.retained_bytes; // ELI5: release the memory counted for its pages.
                it = have_assemblies.erase(it); // ELI5: discard the incomplete checklist.
            } else ++it; // ELI5: leave other peers' checklists in progress.
        }
        for (auto& [key, failed] : failed_sources) { // ELI5: clear this peer from every recent failure list.
            (void)key; // ELI5: the piece key is not needed for this cleanup.
            failed.erase(peer); // ELI5: let a future connection try this peer again.
        }
        reconcile_all(); // ELI5: recalculate piece placement after the peer leaves.
    }

    void expire_incomplete_assemblies() {
        const auto now = std::chrono::steady_clock::now(); // ELI5: read one consistent time for expiration checks.
        for (auto it = manifest_assemblies.begin(); it != manifest_assemblies.end();) {
            if (now - it->second.touched >= incomplete_page_ttl) { // ELI5: discard a manifest that has been idle too long.
                pending_manifest_hash_bytes -= it->second.retained_hash_bytes; // ELI5: release the memory its pages used.
                emit(StorageEventKind::assembly_expired, it->first.second, {}, it->first.first,
                     "expired incomplete manifest page assembly"); // ELI5: tell the application why those pages vanished.
                it = manifest_assemblies.erase(it); // ELI5: remove the expired partial manifest.
            } else ++it; // ELI5: keep a manifest that has recent activity.
        }
        for (auto it = have_assemblies.begin(); it != have_assemblies.end();) {
            if (now - it->second.touched >= incomplete_page_ttl) { // ELI5: discard a checklist that has been idle too long.
                pending_have_bytes -= it->second.retained_bytes; // ELI5: release the memory its pages used.
                emit(StorageEventKind::assembly_expired, it->first.second, {}, it->first.first,
                     "expired incomplete HAVE page assembly"); // ELI5: tell the application that the partial checklist expired.
                it = have_assemblies.erase(it); // ELI5: remove the expired checklist pages.
            } else ++it; // ELI5: keep a checklist that has recent activity.
        }
    }
};

FileStore::FileStore(std::filesystem::path data_directory, // ELI5: receive the folder where data should be saved.
                     PeerIdentity self, // ELI5: receive this computer's identity.
                     ITransport& transport, // ELI5: receive a message-sending service.
                     const IPeerDirectory& peers, // ELI5: receive a connected-peer list service.
                     IChunkFetcher& fetcher, // ELI5: receive a missing-piece download service.
                     EventHandler events) // ELI5: optionally receive a storage-event listener.
    : impl_(std::make_shared<Impl>(std::move(data_directory), self, transport, peers, fetcher, // ELI5: build the hidden coordinator state.
                                   std::move(events))) {} // ELI5: pass along the event listener.

FileStore::~FileStore() = default; // ELI5: clean up the hidden state when the store is destroyed.
FileStore::FileStore(FileStore&&) noexcept = default; // ELI5: allow the store's ownership to move.
FileStore& FileStore::operator=(FileStore&&) noexcept = default; // ELI5: allow this object to take another store's ownership.

FileManifest FileStore::share_file(const std::string& source_path, std::string display_name) {
    return impl_->share(source_path, std::move(display_name)); // ELI5: let the hidden coordinator perform file sharing.
}

SyncReport FileStore::synchronize_with_peer(PeerIdentity peer) { return impl_->sync_peer(peer); } // ELI5: ask the hidden coordinator to exchange catalogs with one peer.

void FileStore::on_peer_joined(PeerIdentity peer) {
    (void)impl_->sync_peer(peer); // ELI5: exchange file lists when the peer arrives.
    impl_->reconcile_all(); // ELI5: adjust piece downloads to include the new peer.
}

void FileStore::on_peer_left(PeerIdentity peer) { impl_->peer_left(peer); } // ELI5: remove a departed peer from saved holder state.

MessageDisposition FileStore::on_message(PeerIdentity source,
                                         std::uint8_t type,
                                         const Bytes& payload) {
    if (!impl_ || !impl_->is_active(source)) return MessageDisposition::ignored; // ELI5: ignore messages from unknown or disconnected senders.
    try {
        impl_->handle_message(source, type, payload); // ELI5: decode the message and update file state.
        return MessageDisposition::handled; // ELI5: tell the caller the message was accepted.
    } catch (const wire::ProtocolError& error) {
        impl_->emit(StorageEventKind::malformed_message, FileId{}, {}, source, error.what()); // ELI5: report a protocol-format problem.
        return MessageDisposition::malformed; // ELI5: tell the caller that the message was invalid.
    } catch (const std::exception& error) {
        impl_->emit(StorageEventKind::storage_error, FileId{}, {}, source, error.what()); // ELI5: report another processing or storage problem.
        return MessageDisposition::malformed; // ELI5: do not treat a failed message as handled.
    }
}

void FileStore::retry_pending() { if (impl_) impl_->reconcile_all(); } // ELI5: retry queued piece work if this store still exists.
void FileStore::recheck_placements() { retry_pending(); } // ELI5: use the same check to refresh the placement plan.
void FileStore::on_timer() {
    if (!impl_) return; // ELI5: do nothing if this object has been moved from.
    impl_->expire_incomplete_assemblies(); // ELI5: remove stale partial network messages.
    impl_->reconcile_all(); // ELI5: retry pieces that should be stored here.
}

std::vector<FileManifest> FileStore::catalog() const {
    std::vector<FileManifest> result; // ELI5: prepare the list that will be returned.
    if (!impl_) return result; // ELI5: a moved-from store has no catalog entries.
    result.reserve(impl_->files.size()); // ELI5: make room for all known files.
    for (const auto& [id, record] : impl_->files) {
        (void)id; // ELI5: this loop needs the manifest, not its map key.
        result.push_back(record.manifest); // ELI5: copy each file description into the result.
    }
    return result; // ELI5: give the catalog list to the caller.
}

PlacementPlan FileStore::placement_for(const FileId& file_id) const {
    if (!impl_) throw std::logic_error("FileStore is moved from"); // ELI5: this query needs a live store.
    const auto found = impl_->files.find(file_id); // ELI5: look for the requested file ID.
    if (found == impl_->files.end()) throw std::out_of_range("file ID is not in the catalog"); // ELI5: explain when the file is unknown.
    return make_placement_plan(found->second.manifest, impl_->active_peers()); // ELI5: calculate and return its current peer assignments.
}

std::vector<PeerIdentity> FileStore::confirmed_holders(const ChunkKey& key) const {
    std::vector<PeerIdentity> result; // ELI5: prepare an empty peer list.
    if (!impl_) return result; // ELI5: a moved-from store has no peer information.
    const auto record = impl_->files.find(key.file_id); // ELI5: find the file this piece belongs to.
    if (record == impl_->files.end() || key.index >= record->second.manifest.chunk_count) return result; // ELI5: unknown files or piece numbers have no known holders.
    result = impl_->holders_for(record->second, key.index); // ELI5: collect the active peers that confirmed this piece.
    return result; // ELI5: give the holder list to the caller.
}

bool FileStore::has_chunk(const ChunkKey& key) const {
    return impl_ && impl_->chunks.has_chunk(key); // ELI5: answer false for a moved-from store, otherwise check disk.
}

std::error_code FileStore::read_chunk(const ChunkKey& key, Bytes& output) const {
    if (!impl_) return std::make_error_code(std::errc::operation_canceled); // ELI5: report that a moved-from store cannot read.
    const auto result = impl_->chunks.read_chunk(key, output); // ELI5: load the requested piece and check its saved bytes.
    if (result == std::errc::illegal_byte_sequence ||
        result == std::errc::no_such_file_or_directory) {
        const auto record = impl_->files.find(key.file_id);
        if (record != impl_->files.end() && key.index < record->second.manifest.chunk_count) {
            impl_->update_single_local_chunk(record->second, key.index, false); // ELI5: withdraw our ownership claim if the disk piece is missing or damaged.
            impl_->emit(StorageEventKind::storage_error, key.file_id, key.index, impl_->self,
                        "local chunk is missing or corrupt; its HAVE bit was withdrawn"); // ELI5: tell the application why peers were notified.
        }
    }
    return result; // ELI5: give the bytes or disk error to the caller.
}

std::error_code FileStore::put_verified_chunk(const ChunkKey& key, const Bytes& bytes) {
    if (!impl_) return std::make_error_code(std::errc::operation_canceled); // ELI5: report that a moved-from store cannot write.
    const auto result = impl_->chunks.put_verified_chunk(key, bytes); // ELI5: check the piece's size and hash before saving.
    if (result) return result; // ELI5: give back a verification or disk error immediately.
    const auto record = impl_->files.find(key.file_id); // ELI5: check whether this file has a catalog record.
    if (record != impl_->files.end()) {
        impl_->update_single_local_chunk(record->second, key.index, true); // ELI5: mark the checked piece as present and tell peers.
        impl_->emit(StorageEventKind::chunk_verified, key.file_id, key.index, impl_->self,
                    "chunk verified and atomically stored"); // ELI5: report the successful piece save.
    }
    return {}; // ELI5: an empty error means the write succeeded.
}

std::optional<FileManifest> FileStore::find_manifest(const FileId& file_id) const {
    if (!impl_) return std::nullopt; // ELI5: a moved-from store has no file information.
    const auto found = impl_->files.find(file_id); // ELI5: check the in-memory catalog first.
    if (found != impl_->files.end()) return found->second.manifest; // ELI5: return the catalog's file description if present.
    return impl_->chunks.find_manifest(file_id); // ELI5: otherwise ask the disk store for saved metadata.
}

} // namespace cn::filestore: the M3 file-sharing coordinator is implemented here.
