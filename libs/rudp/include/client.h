#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"

#include <string>

class Client {
private:
    int socket_fd;
    string server_ip;
    int server_port;
    sockaddr_in server_address;
    bool running;
    uint32_t client_sequence;

    Protocol::PeerState server_state;

public:
    Client(const string& ip, int port);
    bool start();
    void run();
    void stop();
    ~Client();
};

#endif