// protocol.h -- the protocol's behaviour: message types, flags, and the (prototype)
// reliable send/receive built on the packet codec in packet.h.
//
// "Reliable" here is still stop-and-wait: send one packet, wait for its ACK, resend on
// timeout. Later stages replace this part with a sliding window, SACK and sessions.
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <vector>

#include "packet.h"

namespace Protocol {

// ---- message type numbers (protocol.md section 6) ----------------------------------------
namespace PacketType {
    constexpr uint8_t ACK               = 0x01;
    constexpr uint8_t JOIN              = 0x10;
    constexpr uint8_t WELCOME           = 0x11;
    constexpr uint8_t HELLO             = 0x12;
    constexpr uint8_t HEARTBEAT         = 0x13;
    constexpr uint8_t LEAVE             = 0x14;
    constexpr uint8_t PEER_LIST_REQUEST = 0x15;
    constexpr uint8_t PEER_LIST_PAGE    = 0x16;
    constexpr uint8_t CHAT              = 0x20;   // the prototype's "DATA" message
    constexpr uint8_t CATALOG_REQUEST   = 0x30;
    constexpr uint8_t FILE_ANNOUNCE     = 0x31;
    constexpr uint8_t MANIFEST_PAGE     = 0x32;
    constexpr uint8_t HAVE_PAGE         = 0x33;
    constexpr uint8_t CHUNK_REQUEST     = 0x50;
    constexpr uint8_t CHUNK_DATA        = 0x51;
    constexpr uint8_t CHUNK_ERROR       = 0x52;
}

// ---- flag bits (protocol.md section 4) -----------------------------------------------------
namespace Flag {
    constexpr uint8_t RELIABLE  = 0x01;   // consumes a sequence number, must be ACKed
    constexpr uint8_t ACK_VALID = 0x02;   // ack / sack / window fields carry data
    constexpr uint8_t PROBE     = 0x04;   // only on ACK packets
}

// ---- everything we remember about the one peer we are talking to ----------------------------
struct PeerState {
    PeerState();                          // picks a random local_id and local_session

    // Identity carried in every packet header.
    uint32_t local_id;                    // our sender_id (random, never 0)
    uint64_t local_session;               // our sender_session (random, never 0)
    uint64_t remote_session = 0;          // the peer's session; 0 until we hear from it

    // Receiving side.
    uint32_t expected_receive_sequence = 1;
    bool has_pending_packet = false;
    Packet pending_packet;
    sockaddr_in pending_sender{};

    // Settings.
    int timeout_ms = 500;                 // wait this long for an ACK before resending
    int max_retries = 5;                  // give up after this many sends
    bool verbose = true;                  // print "Sent DATA..." lines
};

// ---- text <-> payload helpers (payloads are raw bytes, not std::string) -----------------------
inline std::vector<uint8_t> stringToPayload(const std::string& text)
{
    return std::vector<uint8_t>(text.begin(), text.end());
}

inline std::string payloadToString(const std::vector<uint8_t>& payload)
{
    return std::string(payload.begin(), payload.end());
}

// ---- reliable send / receive -----------------------------------------------------------------

// Sends `packet` (only header.seq and payload are used) and waits for its ACK.
// Resends on timeout. Returns false if it gave up or the socket failed.
bool sendReliable(int socket_fd,
                  const sockaddr_in& destination,
                  const Packet& packet,
                  PeerState& peer_state);

// Blocks until the next in-order DATA packet arrives, ACKs it, and returns it.
// Duplicates are re-ACKed but never delivered twice.
bool receiveReliable(int socket_fd,
                     Packet& packet,
                     sockaddr_in& sender,
                     PeerState& peer_state,
                     const sockaddr_in* required_sender = nullptr);

} // namespace Protocol

#endif
