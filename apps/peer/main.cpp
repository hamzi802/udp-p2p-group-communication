#include <iostream>
#include <string_view>
#include "common/build_info.hpp"
#include <string>
#include <vector>
#include "membership/module.hpp"
#include "chat/module.hpp"

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << cn::project_version << '\n';
        return 0;
    }
if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "peer: C++17 project scaffold\n"
                  << "Usage: peer [--help | --version]\n"
                  << "Networking and file-sharing features are not implemented yet.\n"
                  << "See docs/project-plan.md for assigned tasks.\n";
        return 0;
    }
     if (argc != 1) {
        std::cerr << "Unsupported option. Run peer --help.\n";
        return 2;
    }
    std::vector<cn::membership::peer_list> peers;
    cn::membership::peer_list peer1;
     peer1.id = 10;
     peer1.name = "Peer_1";
      peer1.ip_addr = "192.168.10.1";
      peer1.port_no = 80;

cn::membership::add_peer(peers, peer1);
cn::membership::peer_list peer2;
     peer2.id = 20;
     peer2.name = "Peer_2";
      peer2.ip_addr = "192.168.10.2";
        peer2.port_no = 8080;
cn::membership::add_peer(peers, peer2);
cn::membership::peer_list peer3;
     peer3.id = 30;
     peer3.name = "Peer_3";
      peer3.ip_addr = "192.168.10.3";
        peer3.port_no = 8000;
cn::membership::add_peer(peers, peer3);
cn::membership::peer_list peer4;
     peer4.id = 40;
     peer4.name = "Peer_4";
      peer4.ip_addr = "192.168.10.4";
        peer4.port_no = 9000;
cn::membership::add_peer(peers, peer4);


std::string input;
std::getline(std::cin,input);
if(input=="/peers"){
    cn::membership::display_peer(peers);
}
else {
    cn::chat::send_chat(peers, 10, input);
}
    return 0;
}
