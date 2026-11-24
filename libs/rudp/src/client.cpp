#include "client.h"

#include <iostream>
#include <cstring>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

Client::Client(const string& ip, int port)
    : socket_fd(-1),
      server_ip(ip),
      server_port(port),
      running(false),
      client_sequence(1)
{
    memset(&server_address, 0, sizeof(server_address));
}

Client::~Client()
{
    stop();
}

bool Client::start()
{
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd < 0) {
        cerr << "Failed to create client socket\n";
        return false;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    if (inet_pton(
        AF_INET,
        server_ip.c_str(),
        &server_address.sin_addr
    ) <= 0) {

        cerr << "Invalid server IP address\n";
        close(socket_fd);
        socket_fd = -1;

        return false;
    }

    running = true;

    cout << "Client started\n";
    cout << "Connected to server address "
         << server_ip << ':' << server_port << '\n';

    return true;
}

void Client::run()
{
    if (!running)
        return;

    string message;

    while (running) {

        cout << "\nEnter message (exit to quit): ";

        if (!getline(cin, message))
            break;

        if (message == "exit")
            break;

        if (message.empty())
            continue;

        // NEW: one packet carries at most MAX_PAYLOAD bytes of text.
        if (message.size() > Protocol::MAX_PAYLOAD) {
            cerr << "Message too long (maximum "
                 << Protocol::MAX_PAYLOAD << " bytes)\n";
            continue;
        }

        Protocol::Packet outgoing;

        // CHANGED: the type is set inside sendReliable() now; the field is called seq;
        // the payload is raw bytes, so convert the text.
        outgoing.header.seq = client_sequence;
        outgoing.payload = Protocol::stringToPayload(message);

        const bool sent = Protocol::sendReliable(
            socket_fd,
            server_address,
            outgoing,
            server_state
        );

        if (!sent) {
            cerr << "Message delivery failed\n";
            break;
        }

        client_sequence++;

        // Wait for the server's response.
        Protocol::Packet response;
        sockaddr_in sender{};

        const bool received = Protocol::receiveReliable(
            socket_fd,
            response,
            sender,
            server_state,
            &server_address
        );

        if (received) {
            cout << "Server: "
                 << Protocol::payloadToString(response.payload) << '\n';
        }
    }
}

void Client::stop()
{
    running = false;

    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }

    cout << "Client stopped\n";
}

int main()
{
    Client client("127.0.0.1", 9000);

    if (!client.start())
        return 1;

    client.run();

    return 0;
}
