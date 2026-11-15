#include "membership/module.hpp"
#include <iostream>

namespace cn::membership {

using namespace std;

bool checkduplicate(const vector<peer_list>& peers, int id) {
    for (const auto& peer : peers) {
        if (peer.id == id) {
            return true;
        }
    }
    return false;
}

bool add_peer(vector<peer_list>& peers, const peer_list& per) {
    if (!checkduplicate(peers, per.id)) {
        peers.push_back(per);
        return true;
    }
    return false;
}

bool removepeer_by_id(vector<peer_list>& peers, int id) {
    int pos = 0;
    for (const auto& peer : peers) {
        if (peer.id == id) {
            peers.erase(peers.begin() + pos);
            return true;
        }
        pos++;
    }
    return false;
}

void display_peer(const vector<peer_list>& peers) {
    if (!peers.empty()) {
        cout << "Peers details" << endl;
        for (const auto& peer : peers) {
            cout << peer.id << "|" << peer.ip_addr << "|"
                 << peer.name << "|" << peer.port_no << endl;
        }
        return;
    }
    cout << "Peer list is empty" << endl;
}
bool update_peer(vector<peer_list>& peers, int id,
                 const string& new_ip, int new_port) {
    for (auto& peer : peers) {
        if (peer.id == id) {
            peer.ip_addr = new_ip;
            peer.port_no = new_port;
            return true;
        }
    }
    return false;
}

}