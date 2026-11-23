#include "filestore/chunk_store.hpp" // ELI5: test storage that writes pieces and metadata to disk.
#include "filestore/file_store.hpp" // ELI5: test the coordinator that shares files between peers.
#include "filestore/manifest.hpp" // ELI5: make manifests and check piece sizes.
#include "filestore/placement.hpp" // ELI5: check how pieces are assigned to peers.
#include "filestore/wire.hpp" // ELI5: test encoding and decoding network messages.
#include "hashing/sha256.hpp" // ELI5: create and compare checksums in the test data.

#include <algorithm> // ELI5: find, sort, and count items in test results.
#include <array> // ELI5: hold the simulated group of peers.
#include <chrono> // ELI5: create a unique temporary test folder.
#include <filesystem> // ELI5: create and clean up test files.
#include <fstream> // ELI5: write test inputs and damaged pieces.
#include <iostream> // ELI5: print test success or failure.
#include <memory> // ELI5: own simulated peer stores.
#include <stdexcept> // ELI5: stop a test when an expected behavior is missing.
#include <string> // ELI5: hold error messages and test text.
#include <utility> // ELI5: move test callbacks and byte lists.
#include <vector> // ELI5: hold flexible lists for test inputs and results.

namespace { // ELI5: keep test helpers private to this test program.

using namespace cn; // ELI5: let tests write the shared project types without the cn:: prefix.

constexpr std::size_t initial_peer_count = 4; // ELI5: the distribution scenario starts with four peers.

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message); // ELI5: stop the test with its explanation when a condition is false.
}

template <typename Function>
void require_throws(Function&& function, const std::string& message) {
    bool threw = false; // ELI5: remember whether the tested operation raised an error.
    try { function(); } // ELI5: run the operation that should fail.
    catch (const std::exception&) { threw = true; } // ELI5: mark success if it throws a standard exception.
    require(threw, message); // ELI5: fail the test if no exception happened.
}

struct TemporaryDirectory { // ELI5: give each test run its own scratch folder.
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("cn-filestore-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() { std::filesystem::create_directories(path); } // ELI5: create the scratch folder before tests write files.
    ~TemporaryDirectory() {
        std::error_code ignored; // ELI5: keep cleanup trouble from interrupting test shutdown.
        std::filesystem::remove_all(path, ignored); // ELI5: remove all temporary test files when done.
    }
};

Bytes make_content(std::size_t size) {
    Bytes result(size); // ELI5: make a byte list with the requested length.
    for (std::size_t i = 0; i < size; ++i) result[i] = static_cast<std::uint8_t>((i * 31U + 7U) & 0xffU); // ELI5: fill it with a repeatable pattern.
    return result; // ELI5: return the predictable test contents.
}

void write_bytes(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc); // ELI5: create or replace the test file as raw bytes.
    if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()),
                                     static_cast<std::streamsize>(bytes.size())); // ELI5: write the full byte list.
    if (!output) throw std::runtime_error("could not write test file " + path.string()); // ELI5: stop if writing the test input failed.
}

Bytes chunk_from(const Bytes& bytes, std::uint32_t index) {
    const auto first = static_cast<std::size_t>(index) * filestore::default_chunk_size; // ELI5: calculate where the requested piece begins.
    const auto size = std::min<std::size_t>(filestore::default_chunk_size, bytes.size() - first); // ELI5: use a normal piece length or the remaining final bytes.
    return Bytes(bytes.begin() + static_cast<std::ptrdiff_t>(first),
                 bytes.begin() + static_cast<std::ptrdiff_t>(first + size));
}

FileManifest synthetic_manifest(std::uint32_t chunks) {
    FileManifest manifest; // ELI5: prepare a pretend file description without reading a real file.
    manifest.file_id = hashing::sha256("synthetic manifest identity");
    manifest.display_name = "synthetic.bin";
    manifest.chunk_size = filestore::default_chunk_size;
    manifest.chunk_count = chunks;
    manifest.file_size = static_cast<std::uint64_t>(chunks) * manifest.chunk_size;
    manifest.final_chunk_size = chunks == 0 ? 0 : manifest.chunk_size;
    for (std::uint32_t index = 0; index < chunks; ++index) { // ELI5: make one predictable checksum for every fake piece.
        manifest.chunk_hashes.push_back(hashing::sha256(std::to_string(index))); // ELI5: hash the piece number as simple test data.
    }
    return manifest; // ELI5: return a internally consistent test manifest.
}

// ELI5: cover empty, exact-size, and partial pieces; save them, reopen storage,
// detect damage, and refuse an unsafe display name.
void test_manifest_and_chunk_store(const std::filesystem::path& root) {
    const auto empty_path = root / "empty.bin";
    write_bytes(empty_path, {});
    const auto empty = filestore::build_manifest(empty_path.string());
    require(empty.file_size == 0 && empty.chunk_count == 0 && empty.final_chunk_size == 0,
            "empty file manifest was inconsistent");
    require(empty.file_id == hashing::sha256(Bytes{}), "empty file ID was not SHA-256 of empty content");

    const auto exact_path = root / "exact.bin";
    const auto exact_bytes = make_content(filestore::default_chunk_size);
    write_bytes(exact_path, exact_bytes);
    const auto exact = filestore::build_manifest(exact_path.string());
    require(exact.chunk_count == 1 && exact.final_chunk_size == filestore::default_chunk_size,
            "exact chunk boundary produced the wrong manifest");

    const auto source_path = root / "source.bin";
    const auto source_bytes = make_content(2U * filestore::default_chunk_size + 9U);
    write_bytes(source_path, source_bytes);
    const auto manifest = filestore::build_manifest(source_path.string());
    require(manifest.chunk_count == 3 && manifest.final_chunk_size == 9,
            "partial final chunk length was not recorded exactly");

    const auto store_root = root / "chunk_store";
    filestore::ChunkStore store(store_root);
    store.store_file(source_path.string(), manifest);
    require(store.local_presence(manifest.file_id) == std::vector<std::uint8_t>({1, 1, 1}),
            "stored file did not report all chunks locally verified");
    for (std::uint32_t index = 0; index < manifest.chunk_count; ++index) {
        Bytes actual;
        require(!store.read_chunk(ChunkKey{manifest.file_id, index}, actual),
                "stored chunk failed its verification read");
        require(actual == chunk_from(source_bytes, index), "stored chunk content differed from its source range");
    }

    filestore::ChunkStore reopened(store_root);
    const auto persisted = reopened.find_manifest(manifest.file_id);
    require(persisted.has_value() && persisted->chunk_hashes == manifest.chunk_hashes,
            "manifest did not survive a store reopen");

    const auto corrupt_path = store_root / "chunks" / hashing::to_hex(manifest.file_id) / "1.chunk";
    write_bytes(corrupt_path, Bytes{0, 1, 2});
    Bytes ignored;
    require(reopened.read_chunk(ChunkKey{manifest.file_id, 1}, ignored) == std::errc::illegal_byte_sequence,
            "corrupt stored chunk was not rejected");

    require_throws([&] {
        (void)filestore::build_manifest(source_path.string(), std::string("bad\0name", 8));
    }, "display name containing NUL was accepted");
}

// ELI5: make sure metadata pages survive encode/decode, stay small, preserve bit
// order, and produce the same two-copy placement regardless of peer input order.
void test_wire_and_placement() {
    const auto manifest = synthetic_manifest(70);
    const auto pages = filestore::wire::make_manifest_pages(manifest);
    require(pages.size() == 3, "manifest pagination did not create three pages");
    std::vector<Digest256> reconstructed;
    for (const auto& page : pages) {
        const auto encoded = filestore::wire::encode_manifest_page(page);
        require(encoded.size() <= filestore::wire::max_payload_size,
                "manifest page exceeded the application datagram payload limit");
        const auto decoded = filestore::wire::decode_manifest_page(encoded);
        require(decoded.page_index == page.page_index && decoded.page_count == page.page_count &&
                    decoded.first_chunk_index == page.first_chunk_index && decoded.hashes == page.hashes,
                "manifest page did not round-trip");
        reconstructed.insert(reconstructed.end(), decoded.hashes.begin(), decoded.hashes.end());
    }
    require(reconstructed == manifest.chunk_hashes, "manifest pages changed hash order");

    std::vector<std::uint8_t> have(9000, 0);
    for (std::size_t i = 0; i < have.size(); ++i) have[i] = static_cast<std::uint8_t>((i % 11U) == 0);
    const auto have_pages = filestore::wire::make_have_pages(manifest.file_id, 9, have);
    require(have_pages.size() == 2, "HAVE bitmap pagination did not create two pages");
    for (const auto& page : have_pages) {
        const auto encoded = filestore::wire::encode_have_page(page);
        require(encoded.size() <= filestore::wire::max_payload_size,
                "HAVE page exceeded the application datagram payload limit");
        const auto decoded = filestore::wire::decode_have_page(encoded);
        require(decoded.generation == 9 && decoded.page_index == page.page_index &&
                    decoded.page_count == page.page_count && decoded.bitmap == page.bitmap,
                "HAVE page did not round-trip");
        for (std::uint32_t bit = 0; bit < decoded.bit_count; ++bit) {
            const auto index = decoded.first_chunk_index + bit;
            require(filestore::wire::have_page_contains(decoded, index) == (have[index] != 0),
                    "HAVE bitmap bit order changed during encoding");
        }
    }
    require_throws([] { filestore::wire::decode_catalog_request(Bytes{0}); },
                   "catalog request accepted trailing bytes");

    std::vector<PeerIdentity> peers{{4, 1}, {2, 1}, {1, 1}, {3, 1}};
    auto reversed = peers;
    std::reverse(reversed.begin(), reversed.end());
    const auto plan = filestore::make_placement_plan(manifest, peers);
    const auto same_plan = filestore::make_placement_plan(manifest, reversed);
    require(plan.avoids_a_complete_managed_copy && plan.has_two_distinct_holders,
            "placement did not report its satisfied invariants");
    require(plan.ordered_peers == same_plan.ordered_peers && plan.chunks.size() == same_plan.chunks.size(),
            "placement was not independent of peer input order");
    std::vector<std::uint32_t> counts(peers.size(), 0);
    for (std::size_t i = 0; i < plan.chunks.size(); ++i) {
        const auto& placement = plan.chunks[i];
        require(placement.holders.size() == 2 && placement.holders[0] != placement.holders[1],
                "chunk did not receive two distinct holders");
        require(placement.holders == same_plan.chunks[i].holders,
                "placement assignment changed with input order");
        for (const auto holder : placement.holders) {
            const auto position = std::find(plan.ordered_peers.begin(), plan.ordered_peers.end(), holder);
            require(position != plan.ordered_peers.end(), "placement selected a nonmember peer");
            ++counts[static_cast<std::size_t>(position - plan.ordered_peers.begin())];
        }
    }
    require(std::all_of(counts.begin(), counts.end(), [&](std::uint32_t count) {
        return count < manifest.chunk_count;
    }), "a peer received every managed chunk");
    const auto degraded = filestore::make_placement_plan(manifest, {{1, 1}, {2, 1}});
    require(degraded.has_two_distinct_holders && !degraded.avoids_a_complete_managed_copy,
            "two-peer placement did not report its unavoidable complete copies");
}

struct SentMessage { // ELI5: remember one message sent by a fake transport.
    PeerIdentity target; // ELI5: say who should receive the message.
    std::uint8_t type{}; // ELI5: say what kind of message it is.
    Bytes payload; // ELI5: keep the message bytes.
};

struct FakeTransport final : ITransport { // ELI5: collect messages instead of using a real network.
    std::vector<SentMessage> sent; // ELI5: keep every message the test peer sends.
    SendResult send_reliable(PeerIdentity peer, std::uint8_t type, const Bytes& payload) override {
        sent.push_back(SentMessage{peer, type, payload}); // ELI5: save the send so another test peer can receive it later.
        return SendResult::ok; // ELI5: pretend the network accepted the message.
    }
};

struct FakePeers final : IPeerDirectory { // ELI5: provide a test-controlled list of connected peers.
    std::vector<PeerIdentity> peers; // ELI5: let each scenario choose its active peers.
    std::vector<PeerIdentity> active_peers() const override { return peers; } // ELI5: return that chosen list.
};

struct FakeFetcher final : IChunkFetcher { // ELI5: hold download requests so tests can finish them manually.
    struct Request {
        PeerIdentity source; // ELI5: remember which peer will serve this piece.
        ChunkKey key; // ELI5: remember which file piece was requested.
        std::uint32_t expected_size{}; // ELI5: remember how many bytes the piece should contain.
        Completion completion; // ELI5: remember how to report the eventual download result.
    };
    std::vector<Request> requests; // ELI5: queue downloads for the test to complete.
    bool accept_requests{true}; // ELI5: let a test pretend the transfer queue is full.

    bool fetch_chunk(PeerIdentity source,
                     const ChunkKey& key,
                     std::uint32_t expected_size,
                     Completion completion) override {
        if (!accept_requests) return false; // ELI5: simulate a full queue without saving the request.
        requests.push_back(Request{source, key, expected_size, std::move(completion)}); // ELI5: keep accepted download details and callback.
        return true; // ELI5: tell the file store that the request was queued.
    }

    Request take_next() {
        if (requests.empty()) throw std::runtime_error("no queued fake fetch request"); // ELI5: do not take a request when none exists.
        auto result = std::move(requests.front()); // ELI5: take the oldest queued request.
        requests.erase(requests.begin()); // ELI5: remove it from the pending list.
        return result; // ELI5: let the test supply its downloaded bytes.
    }
};

// ELI5: send a file with enough pieces to need multiple metadata pages, then
// prove that out-of-order HAVE pages are not trusted until the whole snapshot arrives.
void test_large_metadata_and_atomic_have_assembly(const std::filesystem::path& root) {
    const PeerIdentity local{50, 500};
    const PeerIdentity source{60, 600};
    FakeTransport transport;
    FakePeers peers;
    peers.peers = {source};
    FakeFetcher fetcher;
    fetcher.accept_requests = false;
    filestore::FileStore store(root / "large-metadata", local, transport, peers, fetcher);

    const auto manifest = synthetic_manifest(filestore::wire::have_bits_per_page + 1);
    const auto announce = filestore::wire::encode_file_announce(manifest);
    require(announce.size() <= filestore::wire::max_payload_size,
            "file announcement exceeded the application datagram payload limit");
    require(store.on_message(source, message_type::file_announce, announce) ==
                filestore::MessageDisposition::handled,
            "FileStore rejected a valid large-file announcement");

    const auto manifest_pages = filestore::wire::make_manifest_pages(manifest);
    require(manifest_pages.size() > 1, "large manifest did not exercise page assembly");
    for (const auto& page : manifest_pages) {
        const auto payload = filestore::wire::encode_manifest_page(page);
        require(payload.size() <= filestore::wire::max_payload_size,
                "large manifest page exceeded the application datagram payload limit");
        require(store.on_message(source, message_type::manifest_page, payload) ==
                    filestore::MessageDisposition::handled,
                "FileStore rejected a valid manifest page");
    }
    require(store.catalog().size() == 1,
            "FileStore did not assemble all large-manifest pages");

    std::vector<std::uint8_t> bitmap(manifest.chunk_count, 0);
    bitmap.back() = 1;
    const auto have_pages = filestore::wire::make_have_pages(manifest.file_id, 1, bitmap);
    require(have_pages.size() == 2, "large HAVE snapshot did not create multiple pages");
    const auto last_page = filestore::wire::encode_have_page(have_pages.back());
    require(last_page.size() <= filestore::wire::max_payload_size,
            "large HAVE page exceeded the application datagram payload limit");
    require(store.on_message(source, message_type::have_page, last_page) ==
                filestore::MessageDisposition::handled,
            "FileStore rejected a valid out-of-order HAVE page");
    require(store.confirmed_holders(ChunkKey{manifest.file_id, manifest.chunk_count - 1}).empty(),
            "FileStore applied an incomplete HAVE snapshot");

    const auto first_page = filestore::wire::encode_have_page(have_pages.front());
    require(store.on_message(source, message_type::have_page, first_page) ==
                filestore::MessageDisposition::handled,
            "FileStore rejected the final page of a valid HAVE snapshot");
    require(store.confirmed_holders(ChunkKey{manifest.file_id, manifest.chunk_count - 1}) ==
                std::vector<PeerIdentity>{source},
            "FileStore did not atomically apply the complete HAVE snapshot");
}

// ELI5: connect a publisher and downloader using fake services, reject bad bytes,
// retry queued downloads, advertise verified chunks, and repair later corruption.
void test_peer_exchange_and_repair(const std::filesystem::path& root) {
    const PeerIdentity publisher{10, 100};
    const PeerIdentity downloader{20, 200};
    const auto input_path = root / "network-source.bin";
    const auto source_bytes = make_content(2U * filestore::default_chunk_size + 3U);
    write_bytes(input_path, source_bytes);

    FakeTransport publisher_transport;
    FakePeers publisher_peers;
    publisher_peers.peers = {downloader};
    FakeFetcher publisher_fetcher;
    filestore::FileStore publisher_store(root / "publisher", publisher,
                                         publisher_transport, publisher_peers, publisher_fetcher);
    const auto manifest = publisher_store.share_file(input_path.string());
    require(publisher_transport.sent.size() >= 3, "publisher did not send announce, manifest and HAVE messages");

    FakeTransport downloader_transport;
    FakePeers downloader_peers;
    downloader_peers.peers = {publisher};
    FakeFetcher downloader_fetcher;
    downloader_fetcher.accept_requests = false;
    filestore::FileStore downloader_store(root / "downloader", downloader,
                                          downloader_transport, downloader_peers, downloader_fetcher);
    for (const auto& message : publisher_transport.sent) {
        require(message.target == downloader, "publisher sent metadata to an unexpected peer");
        require(downloader_store.on_message(publisher, message.type, message.payload) ==
                    filestore::MessageDisposition::handled,
                "downloader rejected valid catalog or HAVE metadata");
    }
    require(downloader_store.catalog().size() == 1, "remote manifest was not added to the catalog");
    require(downloader_fetcher.requests.empty(), "full transfer queue accepted a fake request");

    auto invalid_chunk = chunk_from(source_bytes, 0);
    invalid_chunk[0] ^= 0xffU;
    require(downloader_store.put_verified_chunk(ChunkKey{manifest.file_id, 0}, invalid_chunk) ==
                std::errc::illegal_byte_sequence,
            "FileStore accepted downloaded bytes with the wrong chunk hash");
    require(!downloader_store.has_chunk(ChunkKey{manifest.file_id, 0}),
            "FileStore stored an unverified downloaded chunk");
    const auto before_verified_fetch = downloader_store.confirmed_holders(ChunkKey{manifest.file_id, 0});
    require(std::find(before_verified_fetch.begin(), before_verified_fetch.end(), downloader) ==
                before_verified_fetch.end(),
            "FileStore advertised a chunk before verified storage");

    downloader_fetcher.accept_requests = true;
    downloader_store.retry_pending();
    require(downloader_fetcher.requests.size() == manifest.chunk_count,
            "downloader did not retry and enqueue every missing placement chunk");

    while (!downloader_fetcher.requests.empty()) {
        auto request = downloader_fetcher.take_next();
        require(request.source == publisher, "download used an unconfirmed source");
        auto bytes = chunk_from(source_bytes, request.key.index);
        require(bytes.size() == request.expected_size, "fetcher received the wrong expected chunk size");
        request.completion(ChunkFetchResult{{}, std::move(bytes)});
    }
    for (std::uint32_t index = 0; index < manifest.chunk_count; ++index) {
        Bytes actual;
        require(!downloader_store.read_chunk(ChunkKey{manifest.file_id, index}, actual),
                "downloaded chunk failed local verification");
        require(actual == chunk_from(source_bytes, index), "downloaded chunk differed from source bytes");
    }
    require(std::any_of(downloader_transport.sent.begin(), downloader_transport.sent.end(), [](const auto& message) {
        return message.type == message_type::have_page;
    }), "downloader did not advertise verified storage");

    const auto damaged = root / "downloader" / "chunk_store" / "chunks" /
        hashing::to_hex(manifest.file_id) / "1.chunk";
    write_bytes(damaged, Bytes{0});
    Bytes ignored;
    require(downloader_store.read_chunk(ChunkKey{manifest.file_id, 1}, ignored) ==
                std::errc::illegal_byte_sequence,
            "FileStore did not reject a corrupt local chunk");
    require(downloader_fetcher.requests.size() == 1,
            "FileStore did not schedule repair from the remaining confirmed holder");
    auto repair = downloader_fetcher.take_next();
    repair.completion(ChunkFetchResult{{}, chunk_from(source_bytes, repair.key.index)});
    Bytes repaired;
    require(!downloader_store.read_chunk(ChunkKey{manifest.file_id, 1}, repaired) &&
                repaired == chunk_from(source_bytes, 1),
            "FileStore did not repair the corrupt chunk");
}

// ELI5: check the warning for too few peers and ensure a receiver keeps file facts
// while reporting that its only piece source has disappeared.
void test_degraded_and_unavailable_events(const std::filesystem::path& root) {
    const auto input_path = root / "unavailable-source.bin";
    write_bytes(input_path, make_content(2U * filestore::default_chunk_size + 3U));

    std::vector<filestore::StorageEvent> isolated_events;
    const PeerIdentity isolated{70, 700};
    FakeTransport isolated_transport;
    FakePeers isolated_peers;
    FakeFetcher isolated_fetcher;
    filestore::FileStore isolated_store(
        root / "isolated", isolated, isolated_transport, isolated_peers, isolated_fetcher,
        [&](const filestore::StorageEvent& event) { isolated_events.push_back(event); });
    (void)isolated_store.share_file(input_path.string());
    require(std::any_of(isolated_events.begin(), isolated_events.end(), [](const auto& event) {
        return event.kind == filestore::StorageEventKind::insufficient_peers;
    }), "M3 did not report that one peer cannot provide two distinct holders");

    const PeerIdentity publisher{71, 710};
    const PeerIdentity receiver{72, 720};
    FakeTransport publisher_transport;
    FakePeers publisher_peers;
    publisher_peers.peers = {receiver};
    FakeFetcher publisher_fetcher;
    filestore::FileStore publisher_store(root / "unavailable-publisher", publisher,
                                         publisher_transport, publisher_peers, publisher_fetcher);
    const auto manifest = publisher_store.share_file(input_path.string());

    std::vector<filestore::StorageEvent> receiver_events;
    FakeTransport receiver_transport;
    FakePeers receiver_peers;
    receiver_peers.peers = {publisher};
    FakeFetcher receiver_fetcher;
    receiver_fetcher.accept_requests = false;
    filestore::FileStore receiver_store(
        root / "unavailable-receiver", receiver, receiver_transport, receiver_peers,
        receiver_fetcher,
        [&](const filestore::StorageEvent& event) { receiver_events.push_back(event); });
    for (const auto& message : publisher_transport.sent) {
        require(receiver_store.on_message(publisher, message.type, message.payload) ==
                    filestore::MessageDisposition::handled,
                "receiver rejected metadata before the source-departure check");
    }
    receiver_peers.peers.clear();
    receiver_store.on_peer_left(publisher);
    require(std::any_of(receiver_events.begin(), receiver_events.end(), [](const auto& event) {
        return event.kind == filestore::StorageEventKind::replica_unavailable;
    }), "M3 did not report replicas unavailable after the only source left");
    require(receiver_store.catalog().size() == 1 &&
                receiver_store.catalog().front().file_id == manifest.file_id,
            "M3 discarded catalog metadata when its only source left");
}

struct SimulatedPeer { // ELI5: collect all fake services and storage for one simulated peer.
    PeerIdentity identity; // ELI5: tell peers apart in the simulation.
    FakeTransport transport; // ELI5: collect messages this peer sends.
    FakePeers peers; // ELI5: provide this peer's active-member list.
    FakeFetcher fetcher; // ELI5: queue this peer's chunk downloads.
    std::unique_ptr<filestore::FileStore> store; // ELI5: run the real M3 file store for this simulated peer.
};

using SimulatedNetwork = std::array<SimulatedPeer, 5>; // ELI5: reserve room for four peers and one newcomer.

SimulatedPeer* find_simulated_peer(SimulatedNetwork& network, PeerIdentity identity) {
    const auto found = std::find_if(network.begin(), network.end(), [&](const SimulatedPeer& peer) {
        return peer.identity == identity;
    });
    return found == network.end() ? nullptr : &*found;
}

// ELI5: repeatedly deliver queued messages and satisfy fake downloads until
// the peers have no work left, or fail if they get stuck.
void drive_simulated_network(SimulatedNetwork& network,
                             const std::vector<std::size_t>& active_indexes) {
    for (std::size_t round = 0; round < 10000; ++round) {
        bool progressed = false;
        for (const auto sender_index : active_indexes) {
            auto& sender = network.at(sender_index);
            std::vector<SentMessage> messages;
            messages.swap(sender.transport.sent);
            for (const auto& message : messages) {
                auto* target = find_simulated_peer(network, message.target);
                require(target != nullptr && target->store != nullptr &&
                            std::find(active_indexes.begin(), active_indexes.end(),
                                      static_cast<std::size_t>(target - network.data())) != active_indexes.end(),
                        "M3 sent a message to an inactive simulated peer");
                require(target->store->on_message(sender.identity, message.type, message.payload) ==
                            filestore::MessageDisposition::handled,
                        "simulated peer rejected a valid M3 message");
                progressed = true;
            }
        }

        for (const auto receiver_index : active_indexes) {
            auto& receiver = network.at(receiver_index);
            while (!receiver.fetcher.requests.empty()) {
                auto request = receiver.fetcher.take_next();
                auto* source = find_simulated_peer(network, request.source);
                require(source != nullptr && source->store != nullptr &&
                            std::find(active_indexes.begin(), active_indexes.end(),
                                      static_cast<std::size_t>(source - network.data())) != active_indexes.end(),
                        "M3 selected an inactive simulated source");
                Bytes bytes;
                require(!source->store->read_chunk(request.key, bytes),
                        "M3 selected a confirmed source without a readable chunk");
                require(bytes.size() == request.expected_size,
                        "M3 requested a simulated chunk with the wrong expected size");
                request.completion(ChunkFetchResult{{}, std::move(bytes)});
                progressed = true;
            }
        }

        bool pending = false;
        for (const auto index : active_indexes) {
            const auto& peer = network.at(index);
            pending = pending || !peer.transport.sent.empty() || !peer.fetcher.requests.empty();
        }
        if (!pending) return;
        require(progressed, "simulated M3 network stopped with work still pending");
    }
    throw std::runtime_error("simulated M3 network did not converge");
}

// ELI5: check that all peers know the file, no peer keeps every piece, every
// piece has two physical copies, and saved holder records match reality.
void require_distribution_invariants(const SimulatedNetwork& network,
                                    const std::vector<std::size_t>& active_indexes,
                                    const FileId& file_id,
                                    std::uint32_t chunk_count) {
    require(active_indexes.size() >= 3, "distribution check requires at least three active peers");
    const auto& reference = network.at(active_indexes.front());
    const auto plan = reference.store->placement_for(file_id);
    require(plan.has_two_distinct_holders && plan.avoids_a_complete_managed_copy,
            "M3 did not report the required multi-peer placement invariants");

    for (const auto peer_index : active_indexes) {
        const auto& peer = network.at(peer_index);
        const auto catalog = peer.store->catalog();
        require(std::any_of(catalog.begin(), catalog.end(), [&](const FileManifest& manifest) {
            return manifest.file_id == file_id && manifest.chunk_count == chunk_count;
        }), "catalog did not survive on every active simulated peer");

        std::uint32_t local_chunks = 0;
        for (std::uint32_t chunk = 0; chunk < chunk_count; ++chunk) {
            if (peer.store->has_chunk(ChunkKey{file_id, chunk})) ++local_chunks;
        }
        require(local_chunks < chunk_count,
                "a simulated peer holds every chunk in managed storage");
    }

    for (std::uint32_t chunk = 0; chunk < chunk_count; ++chunk) {
        std::vector<PeerIdentity> actual_holders;
        for (const auto peer_index : active_indexes) {
            const auto& peer = network.at(peer_index);
            const ChunkKey key{file_id, chunk};
            if (peer.store->has_chunk(key)) actual_holders.push_back(peer.identity);
        }
        require(actual_holders.size() == 2,
                "a distributed chunk does not have exactly two physical holders");

        auto expected_holders = plan.chunks.at(chunk).holders;
        auto reported_holders = reference.store->confirmed_holders(ChunkKey{file_id, chunk});
        std::sort(actual_holders.begin(), actual_holders.end());
        std::sort(expected_holders.begin(), expected_holders.end());
        std::sort(reported_holders.begin(), reported_holders.end());
        require(actual_holders == expected_holders,
                "physical chunk holders do not match the deterministic placement plan");
        require(reported_holders == expected_holders,
                "confirmed holder records do not match verified physical storage");
    }
}

// ELI5: simulate the proposal's 2 MiB file across four peers, add a fifth peer
// while its queue is full, then retry and remove the original publisher safely.
void test_four_peer_distribution_and_publisher_departure(const std::filesystem::path& root) {
    const auto source_bytes = make_content(2U * 1024U * 1024U);
    const auto source_path = root / "four-peer-source.bin";
    write_bytes(source_path, source_bytes);

    SimulatedNetwork network;
    for (std::size_t i = 0; i < network.size(); ++i) {
        network[i].identity = PeerIdentity{static_cast<PeerId>(i + 1), 100 + i};
    }
    for (std::size_t i = 0; i < initial_peer_count; ++i) {
        for (std::size_t j = 0; j < initial_peer_count; ++j) {
            if (i != j) network[i].peers.peers.push_back(network[j].identity);
        }
    }
    for (std::size_t i = 0; i < network.size(); ++i) {
        network[i].store = std::make_unique<filestore::FileStore>(
            root / ("four-peer-" + std::to_string(i)), network[i].identity,
            network[i].transport, network[i].peers, network[i].fetcher);
    }

    const auto manifest = network[0].store->share_file(source_path.string());
    require(manifest.file_size == 2U * 1024U * 1024U && manifest.chunk_count == 128,
            "the M3 four-peer scenario did not use the proposal's 2 MiB file size");
    drive_simulated_network(network, {0, 1, 2, 3});
    require_distribution_invariants(network, {0, 1, 2, 3}, manifest.file_id, manifest.chunk_count);

    std::vector<std::vector<PeerIdentity>> original_holders(manifest.chunk_count);
    for (std::uint32_t chunk = 0; chunk < manifest.chunk_count; ++chunk) {
        for (std::size_t peer_index = 0; peer_index < initial_peer_count; ++peer_index) {
            if (network[peer_index].store->has_chunk(ChunkKey{manifest.file_id, chunk})) {
                original_holders[chunk].push_back(network[peer_index].identity);
            }
        }
    }

    const auto newcomer_index = initial_peer_count;
    network[newcomer_index].fetcher.accept_requests = false;
    for (std::size_t i = 0; i < initial_peer_count; ++i) {
        network[i].peers.peers.push_back(network[newcomer_index].identity);
        network[newcomer_index].peers.peers.push_back(network[i].identity);
        network[i].store->on_peer_joined(network[newcomer_index].identity);
    }
    const auto sync = network[newcomer_index].store->synchronize_with_peer(network[0].identity);
    require(sync.complete(), "new M3 peer could not queue a catalog synchronization");
    const std::vector<std::size_t> five_peers{0, 1, 2, 3, 4};
    drive_simulated_network(network, five_peers);

    const auto newcomer_plan = network[newcomer_index].store->placement_for(manifest.file_id);
    for (std::uint32_t chunk = 0; chunk < manifest.chunk_count; ++chunk) {
        const auto& new_targets = newcomer_plan.chunks.at(chunk).holders;
        if (std::find(new_targets.begin(), new_targets.end(), network[newcomer_index].identity) ==
            new_targets.end()) continue;
        require(!network[newcomer_index].store->has_chunk(ChunkKey{manifest.file_id, chunk}),
                "queue-full newcomer unexpectedly stored a replacement chunk");
        for (const auto old_holder : original_holders[chunk]) {
            const auto* holder = find_simulated_peer(network, old_holder);
            require(holder != nullptr && holder->store->has_chunk(ChunkKey{manifest.file_id, chunk}),
                    "an old verified copy was pruned before the new holder confirmed storage");
        }
    }

    network[newcomer_index].fetcher.accept_requests = true;
    network[newcomer_index].store->retry_pending();
    drive_simulated_network(network, five_peers);
    require_distribution_invariants(network, five_peers, manifest.file_id, manifest.chunk_count);

    const auto departed = network[0].identity;
    const std::vector<std::size_t> survivors{1, 2, 3, 4};
    for (const auto index : survivors) {
        auto& peer = network[index];
        peer.peers.peers.erase(std::remove(peer.peers.peers.begin(), peer.peers.peers.end(), departed),
                               peer.peers.peers.end());
        peer.store->on_peer_left(departed);
    }
    drive_simulated_network(network, survivors);
    require_distribution_invariants(network, survivors, manifest.file_id, manifest.chunk_count);
}

} // ELI5: finish the private test helpers and scenarios.

int main() { // ELI5: run every file-storage behavior check.
    try {
        TemporaryDirectory temporary; // ELI5: make one scratch folder shared by all scenarios.
        test_manifest_and_chunk_store(temporary.path);
        test_wire_and_placement();
        test_large_metadata_and_atomic_have_assembly(temporary.path);
        test_peer_exchange_and_repair(temporary.path);
        test_degraded_and_unavailable_events(temporary.path);
        test_four_peer_distribution_and_publisher_departure(temporary.path);
        std::cout << "filestore behavioral checks passed\n"; // ELI5: report that every scenario succeeded.
        return 0; // ELI5: signal success to the build system.
    } catch (const std::exception& error) {
        std::cerr << "filestore test failed: " << error.what() << '\n'; // ELI5: explain which condition or operation failed.
        return 1; // ELI5: signal test failure to the build system.
    }
}
