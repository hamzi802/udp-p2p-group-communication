#include "protocol.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

namespace Protocol {

static void writeUInt16(uint8_t* buffer, uint16_t value)
{
    buffer[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[1] = static_cast<uint8_t>(value & 0xFF);
}

static void writeUInt32(uint8_t* buffer, uint32_t value)
{
    buffer[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    buffer[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buffer[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[3] = static_cast<uint8_t>(value & 0xFF);
}

static uint16_t readUInt16(const uint8_t* buffer)
{
    return (static_cast<uint16_t>(buffer[0]) << 8) |
           static_cast<uint16_t>(buffer[1]);
}

static uint32_t readUInt32(const uint8_t* buffer)
{
    return (static_cast<uint32_t>(buffer[0]) << 24) |
           (static_cast<uint32_t>(buffer[1]) << 16) |
           (static_cast<uint32_t>(buffer[2]) << 8) |
           static_cast<uint32_t>(buffer[3]);
}

static size_t buildPacket(
    const Packet& packet,
    uint8_t* output)
{
    const size_t payload_length = packet.payload.size();

    // Magic number: bytes 0-1
    writeUInt16(output, MAGIC_NUMBER);

    // Version: byte 2
    output[2] = PROTOCOL_VERSION;

    // Packet type: byte 3
    output[3] = static_cast<uint8_t>(packet.header.type);

    // Sequence number: bytes 4-7
    writeUInt32(output + 4, packet.header.sequence_number);

    // Payload length: bytes 8-9
    writeUInt16(output + 8, static_cast<uint16_t>(payload_length));

    // Payload starts at byte 10
    if (payload_length > 0) {
        copy(
            packet.payload.begin(),
            packet.payload.end(),
            output + HEADER_SIZE
        );
    }

    return HEADER_SIZE + payload_length;
}

static ParseResult parsePacket(
    const uint8_t* buffer,
    size_t buffer_length,
    Packet& packet)
{
    if (buffer_length < HEADER_SIZE)
        return PARSE_TOO_SHORT;

    const uint16_t magic = readUInt16(buffer);

    if (magic != MAGIC_NUMBER)
        return PARSE_BAD_MAGIC;

    const uint8_t version = buffer[2];

    if (version != PROTOCOL_VERSION)
        return PARSE_BAD_VERSION;

    const uint8_t type = buffer[3];

    if (type != static_cast<uint8_t>(PacketType::DATA) &&
        type != static_cast<uint8_t>(PacketType::ACK)) {
        return PARSE_BAD_TYPE;
    }

    const uint32_t sequence_number = readUInt32(buffer + 4);
    const uint16_t payload_length = readUInt16(buffer + 8);

    if (payload_length > MAX_PAYLOAD_SIZE || buffer_length != HEADER_SIZE + payload_length) {
        return PARSE_BAD_LENGTH;
    }

    if (type == static_cast<uint8_t>(PacketType::ACK) && payload_length != 0) {
        return PARSE_BAD_LENGTH;
    }

    packet.header.magic = magic;
    packet.header.version = version;
    packet.header.type = static_cast<PacketType>(type);
    packet.header.sequence_number = sequence_number;
    packet.header.payload_length = payload_length;

    packet.payload.assign(
        reinterpret_cast<const char*>(buffer + HEADER_SIZE),
        payload_length
    );

    return PARSE_OK;
}

bool encodePacket(
    const Packet& packet,
    uint8_t* output,
    size_t output_capacity,
    size_t& output_length)
{
    output_length = 0;

    if (output == nullptr)
        return false;

    if (packet.payload.size() > MAX_PAYLOAD_SIZE){
        cout<<"payload size increased"<<endl;
        return false;
    }

    if (output_capacity < HEADER_SIZE + packet.payload.size())
        return false;

    if (packet.header.type != PacketType::DATA && packet.header.type != PacketType::ACK) {
        return false;
    }

    if (packet.header.type == PacketType::ACK && !packet.payload.empty()) {
        return false;
    }

    output_length = buildPacket(packet, output);

    return true;
}

ParseResult decodePacket(
    const uint8_t* buffer,
    size_t buffer_length,
    Packet& packet)
{
    if (buffer == nullptr)
        return PARSE_TOO_SHORT;

    return parsePacket(buffer, buffer_length, packet);
}

const char* parseResultText(ParseResult result)
{
    switch (result) {
        case PARSE_OK:
            return "Packet parsed successfully";

        case PARSE_TOO_SHORT:
            return "Packet is too short";

        case PARSE_BAD_MAGIC:
            return "Invalid magic number";

        case PARSE_BAD_VERSION:
            return "Unsupported protocol version";

        case PARSE_BAD_LENGTH:
            return "Invalid payload length";

        case PARSE_BAD_TYPE:
            return "Unknown packet type";
    }

    return "Unknown parsing error";
}

const char* packetTypeName(PacketType type)
{
    switch (type) {
        case PacketType::DATA:
            return "DATA";

        case PacketType::ACK:
            return "ACK";
    }

    return "UNKNOWN";
}

static bool sameAddress(
    const sockaddr_in& first,
    const sockaddr_in& second)
{
    return first.sin_family == second.sin_family &&
           first.sin_port == second.sin_port &&
           first.sin_addr.s_addr == second.sin_addr.s_addr;
}

static bool sendPacket(
    int socket_fd,
    const sockaddr_in& destination,
    const Packet& packet)
{
    uint8_t buffer[MAX_PACKET_SIZE];
    size_t packet_length = 0;

    if (!encodePacket(packet, buffer, sizeof(buffer), packet_length))
        return false;

    const ssize_t sent = sendto(
        socket_fd,
        buffer,
        packet_length,
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination)
    );

    return sent == static_cast<ssize_t>(packet_length);
}

static bool sendACK(
    int socket_fd,
    const sockaddr_in& destination,
    uint32_t sequence_number)
{
    Packet ack;

    ack.header.type = PacketType::ACK;
    ack.header.sequence_number = sequence_number;

    return sendPacket(socket_fd, destination, ack);
}

// Acknowledge incoming DATA and remember it for the application.
static void handleIncomingData(
    int socket_fd,
    const Packet& packet,
    const sockaddr_in& sender,
    PeerState& peer_state)
{
    const uint32_t sequence = packet.header.sequence_number;

    // Old packet: it was already delivered.
    // ACK it again because the previous ACK may have been lost.
    if (sequence < peer_state.expected_receive_sequence) {
        sendACK(socket_fd, sender, sequence);
        return;
    }

    // Accept only the next expected packet.
    if (sequence == peer_state.expected_receive_sequence &&
        !peer_state.has_pending_packet) {

        peer_state.pending_packet = packet;
        peer_state.pending_sender = sender;
        peer_state.has_pending_packet = true;

        peer_state.expected_receive_sequence++;
    }

    // ACK accepted packets and duplicates.
    if (sequence <= peer_state.expected_receive_sequence) {
        sendACK(socket_fd, sender, sequence);
    }
}

bool sendReliable(
    int socket_fd,
    const sockaddr_in& destination,
    const Packet& packet,
    PeerState& peer_state)
{
    constexpr int MAX_RETRIES = 5;
    constexpr int TIMEOUT_MS = 500;

    Packet outgoing = packet;
    outgoing.header.type = PacketType::DATA;

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {

        if (!sendPacket(socket_fd, destination, outgoing)) {
            cerr << "Failed to send DATA packet\n";
            return false;
        }

        cout << "Sent DATA, sequence "
                  << outgoing.header.sequence_number
                  << ", attempt " << attempt << '\n';

        while (true) {

            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(socket_fd, &read_fds);

            timeval timeout{};
            timeout.tv_sec = TIMEOUT_MS / 1000;
            timeout.tv_usec = (TIMEOUT_MS % 1000) * 1000;

            const int ready = select(
                socket_fd + 1,
                &read_fds,
                nullptr,
                nullptr,
                &timeout
            );

            if (ready == 0) {
                cout << "ACK timeout. Retrying...\n";
                break;
            }

            if (ready < 0) {
                cerr << "select() failed\n";
                return false;
            }

            uint8_t buffer[MAX_PACKET_SIZE];
            sockaddr_in sender{};
            socklen_t sender_length = sizeof(sender);

            const ssize_t received = recvfrom(
                socket_fd,
                buffer,
                sizeof(buffer),
                0,
                reinterpret_cast<sockaddr*>(&sender),
                &sender_length
            );

            if (received <= 0)
                continue;

            if (!sameAddress(sender, destination))
                continue;

            Packet incoming;

            const ParseResult result = decodePacket(
                buffer,
                static_cast<size_t>(received),
                incoming
            );

            if (result != PARSE_OK)
                continue;

            // Check whether this is the ACK we are waiting for.
            if (incoming.header.type == PacketType::ACK) {

                if (incoming.header.sequence_number ==
                    outgoing.header.sequence_number) {

                    cout << "Received ACK for sequence "
                              << incoming.header.sequence_number
                              << '\n';

                    return true;
                }

                continue;
            }

            // Do not discard DATA arriving while waiting for ACK.
            if (incoming.header.type == PacketType::DATA) {
                handleIncomingData(
                    socket_fd,
                    incoming,
                    sender,
                    peer_state
                );
            }
        }
    }

    cerr << "Maximum retries reached. Sending failed.\n";

    return false;
}

bool receiveReliable(
    int socket_fd,
    Packet& packet,
    sockaddr_in& sender,
    PeerState& peer_state,
    const sockaddr_in* required_sender)
{
    // Deliver previously buffered DATA first.
    if (peer_state.has_pending_packet) {

        packet = move(peer_state.pending_packet);
        sender = peer_state.pending_sender;

        peer_state.has_pending_packet = false;

        return true;
    }

    while (true) {

        uint8_t buffer[MAX_PACKET_SIZE];
        sockaddr_in incoming_sender{};
        socklen_t sender_length = sizeof(incoming_sender);

        const ssize_t received = recvfrom(
            socket_fd,
            buffer,
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&incoming_sender),
            &sender_length
        );

        if (received <= 0)
            continue;

        // Optionally accept packets from only one peer.
        if (required_sender != nullptr &&
            !sameAddress(incoming_sender, *required_sender)) {
            continue;
        }

        Packet incoming;

        const ParseResult result = decodePacket(
            buffer,
            static_cast<size_t>(received),
            incoming
        );

        if (result != PARSE_OK) {
            cerr << "Invalid packet: "
                      << parseResultText(result) << '\n';
            continue;
        }

        // ACK packets are handled by sendReliable().
        if (incoming.header.type == PacketType::ACK)
            continue;

        const uint32_t sequence = incoming.header.sequence_number;

        // Duplicate packet: ACK again, but do not deliver twice.
        if (sequence < peer_state.expected_receive_sequence) {
            sendACK(socket_fd, incoming_sender, sequence);
            continue;
        }

        // Ignore packets that are ahead of the expected sequence.
        if (sequence > peer_state.expected_receive_sequence)
            continue;

        // Accept the expected DATA packet.
        sendACK(socket_fd, incoming_sender, sequence);

        peer_state.expected_receive_sequence++;

        packet = move(incoming);
        sender = incoming_sender;

        return true;
    }
}

}