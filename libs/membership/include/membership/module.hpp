#pragma once

#include <string>
#include <vector>
namespace cn::membership {

struct peer_list {
    int id;
    std::string name;
    std::string ip_addr;
    int port_no;
};
bool checkduplicate(const std::vector<peer_list>& peers, int id);

bool add_peer(std::vector<peer_list>& peers, const peer_list& per);

bool removepeer_by_id(std::vector<peer_list>& peers, int id);

void display_peer(const std::vector<peer_list>& peers);

bool update_peer(std::vector<peer_list>& peers, int id,
                 const std::string& new_ip, int new_port);

}