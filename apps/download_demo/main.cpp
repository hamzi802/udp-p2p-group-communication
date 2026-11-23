#include <iostream> // ELI5: print messages to the terminal.
#include <string_view> // ELI5: compare command-line words without copying them.
#include "common/build_info.hpp" // ELI5: display the shared version number.

int main(int argc, char* argv[]) { // ELI5: start the download placeholder program.
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << cn::project_version << '\n'; // ELI5: print the project version when requested.
        return 0; // ELI5: report that the command succeeded.
    }
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "download_demo: C++17 project scaffold\n" // ELI5: explain this is only a starter program...
                  << "Usage: download_demo [--help | --version]\n"
                  << "Networking and file-sharing features are not implemented yet.\n"
                  << "See docs/project-plan.md for assigned tasks.\n"; // ELI5: point developers to the project task list.
        return 0; // ELI5: help output is a successful command.
    }
    std::cerr << "Unsupported option. Run download_demo --help.\n"; // ELI5: explain how to see supported commands.
    return 2; // ELI5: a nonzero status means the option was not understood.
}
