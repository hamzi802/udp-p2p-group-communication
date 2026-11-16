#include "chat/module.hpp"
#include <iostream>

namespace cn::chat {

using cn::membership::peer_list;// Define the peer_list type from the membership module
using namespace std;

void receive_chat(const peer_list& reciver_peer, int sender_id,
                  const string& message) {
    cout << reciver_peer.name << " received from "
         << sender_id << " " << message << endl;
}

void send_chat(const vector<peer_list>& peers, int my_id,
               const string& message) {
    for (const auto& peer : peers) {
        if (peer.id != my_id) {
            cout << peer.ip_addr << ":" << peer.port_no << endl;
            receive_chat(peer, my_id, message);
        }
    }
}

}