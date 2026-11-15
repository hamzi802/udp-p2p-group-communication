#pragma once

#include <string>
#include <vector>
#include "membership/module.hpp"

namespace cn::chat {

void receive_chat(const cn::membership::peer_list& receiver_peer,
                  int sender_id,
                  const std::string& message);

void send_chat(const std::vector<cn::membership::peer_list>& peers,
               int my_id,
               const std::string& message);

}