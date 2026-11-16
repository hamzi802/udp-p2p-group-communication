#include "server.h"

#include <iostream>
#include <cstring>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

Server::Server(int port)
    : socket_fd(-1),
      port(port),
      running(false),
      client_connected(false),
      server_sequence(1)
{
    memset(&client_address, 0, sizeof(client_address));
}

Server::~Server()
{
    stop();
}

bool Server::start()
{
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd < 0) {
        cerr << "Failed to create server socket\n";
        return false;
    }

    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(
        socket_fd,
        reinterpret_cast<sockaddr*>(&server_address),
        sizeof(server_address)
    ) < 0) {

        cerr << "Failed to bind server socket\n";
        close(socket_fd);
        socket_fd = -1;

        return false;
    }

    running = true;

    cout << "Server listening on UDP port "
              << port << '\n';

    return true;
}

void Server::run()
{
    if (!running)
        return;

    while (running) {

        Protocol::Packet incoming;
        sockaddr_in sender{};

        const bool received = Protocol::receiveReliable(
            socket_fd,
            incoming,
            sender,
            client_state,
            client_connected ? &client_address : nullptr
        );

        if (!received)
            continue;

        // The first peer becomes the connected client.
        if (!client_connected) {
            client_address = sender;
            client_connected = true;

            cout << "Client connected: "
                      << inet_ntoa(sender.sin_addr)
                      << ':' << ntohs(sender.sin_port)
                      << '\n';
        }

        cout << "Client: " << incoming.payload << '\n';

        // Echo the received message.
        Protocol::Packet response;

        response.header.type = Protocol::PacketType::DATA;
        response.header.sequence_number = server_sequence;
        response.payload = "Server received: " + incoming.payload;

        const bool sent = Protocol::sendReliable(
            socket_fd,
            client_address,
            response,
            client_state
        );

        if (sent) {
            server_sequence++;
        } else {
            cerr << "Failed to deliver response. Stopping server.\n";
            running = false;
        }
    }
}

void Server::stop()
{
    running = false;

    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }

    cout << "Server stopped\n";
}

int main()
{
    Server server(9000);

    if (!server.start())
        return 1;

    server.run();

    return 0;
}