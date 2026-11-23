#pragma once // ELI5: include this small header only once.
#include <string_view> // ELI5: use lightweight read-only text values.

namespace cn { // ELI5: keep the project labels in the shared namespace.
inline constexpr std::string_view project_name = "CN UDP P2P"; // ELI5: the human-friendly project name.
inline constexpr std::string_view project_version = "0.1.0"; // ELI5: the version label shown by the build.
} // namespace cn: the shared project labels are ready.
