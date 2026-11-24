#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

#include "protocol.h"

class Server {
private:
    int socket_fd;
    int port;
    bool running;
    bool client_connected;

    sockaddr_in client_address;
    uint32_t server_sequence;
    Protocol::PeerState client_state;

public:
    explicit Server(int port);
    bool start();
    void run();
    void stop();
    ~Server();
};

#endif
