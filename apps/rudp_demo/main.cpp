#include <iostream>
#include <string_view>
#include "common/build_info.hpp"

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << cn::project_version << '\n';
        return 0;
    }
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "rudp_demo: C++17 project scaffold\n"
                  << "Usage: rudp_demo [--help | --version]\n"
                  << "Networking and file-sharing features are not implemented yet.\n"
                  << "See docs/project-plan.md for assigned tasks.\n";
        return 0;
    }
    std::cerr << "Unsupported option. Run rudp_demo --help.\n";
    return 2;
}
