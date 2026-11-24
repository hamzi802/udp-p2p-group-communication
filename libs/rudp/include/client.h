#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>
#include <string>

#include "protocol.h"

class Client {
private:
    int socket_fd;
    std::string server_ip;
    int server_port;
    sockaddr_in server_address;
    bool running;
    uint32_t client_sequence;

    Protocol::PeerState server_state;

public:
    Client(const std::string& ip, int port);
    bool start();
    void run();
    void stop();
    ~Client();
};

#endif
