#include "protocol.h"

#include <cerrno>
#include <chrono>
#include <iostream>
#include <random>
#include <utility>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>

namespace Protocol {

using std::cerr;
using std::cout;

// ---- identity ---------------------------------------------------------------------------

static uint32_t randomNonZero32()
{
    std::random_device rd;
    uint32_t v = 0;
    while (v == 0) v = rd();
    return v;
}

static uint64_t randomNonZero64()
{
    std::random_device rd;
    uint64_t v = 0;
    while (v == 0) v = (static_cast<uint64_t>(rd()) << 32) | rd();
    return v;
}

PeerState::PeerState()
    : local_id(randomNonZero32()),
      local_session(randomNonZero64())
{
}

// ---- small helpers ------------------------------------------------------------------------

static bool sameAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_family == b.sin_family &&
           a.sin_port == b.sin_port &&
           a.sin_addr.s_addr == b.sin_addr.s_addr;
}

// "a comes before b" for 32-bit sequence numbers, safe across wraparound (Stage 6 detail).
static bool seqBefore(uint32_t a, uint32_t b)
{
    return static_cast<int32_t>(a - b) < 0;
}

// TEMPORARY mini-validator: just the rules this prototype needs. Stage 3 replaces it with
// the full list from protocol.md section 7.
static bool prototypeChecksPass(const Packet& packet)
{
    const PacketHeader& h = packet.header;

    if (h.magic != MAGIC || h.version != VERSION || h.reserved != 0) return false;
    if (h.stream != 0) return false;
    if (h.sender_id == 0 || h.sender_session == 0) return false;

    if (h.type == PacketType::CHAT)                       // our DATA message
        return h.flags == Flag::RELIABLE && h.seq != 0 &&
               h.ack == 0 && h.sack == 0 && h.window == 0;

    if (h.type == PacketType::ACK)
        return h.flags == Flag::ACK_VALID && h.seq == 0 && h.window <= 32 &&
               (h.sack & 0x80000000u) == 0 && packet.payload.empty();

    return false;                                          // any other type: ignore
}

// Is this packet part of OUR conversation? Learns the peer's session on first contact,
// then rejects packets from any other session (stale packets from an old run).
static bool belongsToConversation(const Packet& packet, PeerState& state)
{
    const PacketHeader& h = packet.header;

    // receiver_session is 0 only until the peer has heard from us; otherwise it must be ours.
    if (h.receiver_session != 0 && h.receiver_session != state.local_session)
        return false;

    if (state.remote_session == 0) {
        state.remote_session = h.sender_session;
        return true;
    }
    return h.sender_session == state.remote_session;
}

// ---- sending --------------------------------------------------------------------------------

// Fills in the identity fields, encodes (CRC included) and sends. `packet` is a copy, so
// the caller's packet is not modified.
static bool sendPacket(int socket_fd,
                       const sockaddr_in& destination,
                       Packet packet,
                       const PeerState& state)
{
    packet.header.stream = 0;
    packet.header.sender_id = state.local_id;
    packet.header.sender_session = state.local_session;
    packet.header.receiver_session = state.remote_session;

    uint8_t buffer[MAX_DATAGRAM];
    size_t length = 0;

    const EncodeResult result = encodePacket(packet, buffer, sizeof(buffer), length);
    if (result != EncodeResult::Ok) {
        cerr << "Cannot encode packet: " << encodeResultText(result) << '\n';
        return false;
    }

    const ssize_t sent = sendto(socket_fd, buffer, length, 0,
                                reinterpret_cast<const sockaddr*>(&destination),
                                sizeof(destination));
    return sent == static_cast<ssize_t>(length);
}

// ACK = "I have everything below `ack`" (the next sequence number I expect).
static bool sendAck(int socket_fd, const sockaddr_in& destination, const PeerState& state)
{
    Packet ack;
    ack.header.type = PacketType::ACK;
    ack.header.flags = Flag::ACK_VALID;
    ack.header.seq = 0;                                   // ACKs are not reliable: no sequence number
    ack.header.ack = state.expected_receive_sequence;
    ack.header.sack = 0;                                  // SACK comes in Stage 8
    ack.header.window = 1;                                // prototype: one packet at a time
    return sendPacket(socket_fd, destination, ack, state);
}

// DATA that arrives while we are busy waiting for an ACK: keep it for the application.
static void handleIncomingData(int socket_fd,
                               const Packet& packet,
                               const sockaddr_in& sender,
                               PeerState& state)
{
    const uint32_t seq = packet.header.seq;

    // Old packet: already delivered. ACK again because our earlier ACK may have been lost.
    if (seqBefore(seq, state.expected_receive_sequence)) {
        sendAck(socket_fd, sender, state);
        return;
    }

    // The next expected packet, and we have room to keep it: store it and ACK it.
    if (seq == state.expected_receive_sequence && !state.has_pending_packet) {
        state.pending_packet = packet;
        state.pending_sender = sender;
        state.has_pending_packet = true;
        state.expected_receive_sequence++;
        sendAck(socket_fd, sender, state);
        return;
    }

    // Otherwise (ahead of sequence, or no room): do NOT ACK. The sender will resend.
    // (The old code ACKed the "no room" case and lost the packet.)
}

// ---- sendReliable ---------------------------------------------------------------------------

bool sendReliable(int socket_fd,
                  const sockaddr_in& destination,
                  const Packet& packet,
                  PeerState& state)
{
    using Clock = std::chrono::steady_clock;

    Packet outgoing = packet;
    outgoing.header.type = PacketType::CHAT;
    outgoing.header.flags = Flag::RELIABLE;
    outgoing.header.ack = 0;
    outgoing.header.sack = 0;
    outgoing.header.window = 0;

    for (int attempt = 1; attempt <= state.max_retries; attempt++) {

        if (!sendPacket(socket_fd, destination, outgoing, state)) {
            cerr << "Failed to send DATA packet\n";
            return false;
        }

        if (state.verbose)
            cout << "Sent DATA, sequence " << outgoing.header.seq
                 << ", attempt " << attempt << '\n';

        // Wait up to timeout_ms in total. Packets we ignore do not extend the wait.
        const auto deadline = Clock::now() + std::chrono::milliseconds(state.timeout_ms);

        while (true) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  deadline - Clock::now()).count();
            if (left <= 0) {
                if (state.verbose && attempt < state.max_retries)
                    cout << "ACK timeout. Retrying...\n";
                break;
            }

            pollfd pfd{socket_fd, POLLIN, 0};
            const int ready = poll(&pfd, 1, static_cast<int>(left));

            if (ready == 0) {
                if (state.verbose && attempt < state.max_retries)
                    cout << "ACK timeout. Retrying...\n";
                break;
            }
            if (ready < 0) {
                if (errno == EINTR) continue;
                cerr << "poll() failed\n";
                return false;
            }

            uint8_t buffer[RECV_BUFFER];
            sockaddr_in sender{};
            socklen_t sender_length = sizeof(sender);

            const ssize_t received = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                              reinterpret_cast<sockaddr*>(&sender),
                                              &sender_length);
            if (received <= 0) continue;
            if (!sameAddress(sender, destination)) continue;

            Packet incoming;
            if (decodePacket(buffer, static_cast<size_t>(received), incoming) != DecodeResult::Ok)
                continue;                                  // damaged or malformed: drop silently
            if (!prototypeChecksPass(incoming) || !belongsToConversation(incoming, state))
                continue;

            if (incoming.header.type == PacketType::ACK) {
                // ack = "next expected", so the ACK for our packet carries seq + 1.
                if (incoming.header.ack == outgoing.header.seq + 1) {
                    if (state.verbose)
                        cout << "Received ACK for sequence " << outgoing.header.seq << '\n';
                    return true;
                }
                continue;                                  // an older (duplicate) ACK
            }

            // DATA from the peer while we wait: keep it, do not discard it.
            handleIncomingData(socket_fd, incoming, sender, state);
        }
    }

    cerr << "Maximum retries reached. Sending failed.\n";
    return false;
}

// ---- receiveReliable ------------------------------------------------------------------------

bool receiveReliable(int socket_fd,
                     Packet& packet,
                     sockaddr_in& sender,
                     PeerState& state,
                     const sockaddr_in* required_sender)
{
    // Deliver previously buffered DATA first.
    if (state.has_pending_packet) {
        packet = std::move(state.pending_packet);
        sender = state.pending_sender;
        state.has_pending_packet = false;
        return true;
    }

    while (true) {
        uint8_t buffer[RECV_BUFFER];
        sockaddr_in incoming_sender{};
        socklen_t sender_length = sizeof(incoming_sender);

        const ssize_t received = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                          reinterpret_cast<sockaddr*>(&incoming_sender),
                                          &sender_length);
        if (received < 0) {
            if (errno == EINTR) continue;
            return false;                                  // real socket error (or socket closed)
        }
        if (received == 0) continue;

        // Optionally accept packets from only one peer.
        if (required_sender != nullptr && !sameAddress(incoming_sender, *required_sender))
            continue;

        Packet incoming;
        const DecodeResult result = decodePacket(buffer, static_cast<size_t>(received), incoming);
        if (result != DecodeResult::Ok) {
            if (state.verbose)
                cerr << "Invalid packet: " << decodeResultText(result) << '\n';
            continue;
        }

        if (!prototypeChecksPass(incoming) || !belongsToConversation(incoming, state))
            continue;

        // ACK packets are handled by sendReliable().
        if (incoming.header.type != PacketType::CHAT)
            continue;

        const uint32_t seq = incoming.header.seq;

        // Duplicate: ACK again, but do not deliver twice.
        if (seqBefore(seq, state.expected_receive_sequence)) {
            sendAck(socket_fd, incoming_sender, state);
            continue;
        }

        // Ahead of the expected sequence: ignore, the sender will resend in order.
        if (seq != state.expected_receive_sequence)
            continue;

        // The expected DATA packet: count it first, then ACK (the ACK carries "next expected").
        state.expected_receive_sequence++;
        sendAck(socket_fd, incoming_sender, state);

        packet = std::move(incoming);
        sender = incoming_sender;
        return true;
    }
}

} // namespace Protocol
