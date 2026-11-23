#pragma once // ELI5: let the compiler include this file only once.

#include "common/file_types.hpp" // ELI5: use the shared peer, file, and byte types.

#include <cstddef> // ELI5: use standard size numbers.
#include <functional> // ELI5: store callback functions.
#include <optional> // ELI5: represent a value that may be missing.
#include <system_error> // ELI5: return standard error descriptions.
#include <vector> // ELI5: return lists of peers.

namespace cn { // ELI5: keep shared types and interfaces together.

enum class SendResult { // ELI5: list the possible results of sending a message.
    ok, // ELI5: the message was accepted for sending.
    peer_unknown, // ELI5: this peer is not in the transport's list.
    payload_too_large, // ELI5: the message is bigger than the protocol allows.
    queue_full, // ELI5: the send queue has no free space right now.
    link_failed, // ELI5: the network link could not send the message.
    bad_type // ELI5: this message kind is not supported.
};

struct ChunkFetchResult { // ELI5: report downloaded bytes, or why they could not be fetched.
    std::error_code error; // ELI5: an empty error means success.
    Bytes bytes; // ELI5: hold the downloaded piece when successful.
};

class ITransport { // ELI5: describe what any reliable message sender must provide.
public:
    virtual ~ITransport() = default; // ELI5: allow safe cleanup through this interface.

    // Sends one reliable application message; the message type selects its stream.
    // Callers must treat non-ok results as unsent and may retry after transport
    // capacity or link state changes.
    virtual SendResult send_reliable(PeerIdentity peer, // ELI5: say which peer should receive the message.
                                     std::uint8_t type, // ELI5: say what kind of message these bytes contain.
                                     const Bytes& payload) = 0; // ELI5: send the bytes; a concrete transport supplies the network work.
};

class IPeerDirectory { // ELI5: describe how to ask for the currently connected peers.
public:
    virtual ~IPeerDirectory() = default; // ELI5: allow safe cleanup through this interface.

    // Returns active remote peers. The caller adds its own identity when needed.
    virtual std::vector<PeerIdentity> active_peers() const = 0; // ELI5: return a list of connected remote peers.
};

class IChunkStore { // ELI5: describe how to save and look up file pieces.
public:
    virtual ~IChunkStore() = default; // ELI5: allow safe cleanup through this interface.

    virtual bool has_chunk(const ChunkKey& key) const = 0; // ELI5: answer whether this piece is saved here.
    virtual std::error_code read_chunk(const ChunkKey& key, Bytes& output) const = 0; // ELI5: copy a saved piece into output, or return its error.
    virtual std::error_code put_verified_chunk(const ChunkKey& key, // ELI5: identify which piece should be stored.
                                               const Bytes& bytes) = 0; // ELI5: save the bytes after their checksum has been checked.
    virtual std::optional<FileManifest> find_manifest(const FileId& file_id) const = 0; // ELI5: return the file details if they are known.
};

class IChunkFetcher { // ELI5: describe how to request one piece from another peer.
public:
    using Completion = std::function<void(ChunkFetchResult)>; // ELI5: name the function called when fetching finishes.

    virtual ~IChunkFetcher() = default; // ELI5: allow safe cleanup through this interface.

    // Returns false only when the request could not be queued (for example, the
    // bounded transfer queue is full); then completion is not called. A queued
    // request completes exactly once, asynchronously on the PeerApp event-loop
    // thread, after this method has returned.
    virtual bool fetch_chunk(PeerIdentity source, // ELI5: choose which peer will send the piece.
                             const ChunkKey& key, // ELI5: identify the wanted file piece.
                             std::uint32_t expected_size, // ELI5: tell the fetcher the expected byte count.
                             Completion completion) = 0; // ELI5: call this function later with success or failure; false means no request was queued.
};

} // namespace cn: these shared interfaces are now declared.
