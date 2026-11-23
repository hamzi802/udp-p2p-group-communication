#include "common/build_info.hpp" // ELI5: provide the app's version label.
#include "filestore/chunk_store.hpp" // ELI5: save and load local pieces and metadata.
#include "filestore/manifest.hpp" // ELI5: split files into pieces and describe them.
#include "filestore/placement.hpp" // ELI5: demonstrate which pretend peers would hold pieces.
#include "hashing/sha256.hpp" // ELI5: calculate IDs and print checksums.

#include <algorithm> // ELI5: search lists and count true entries.
#include <chrono> // ELI5: give reconstructed output a unique temporary name.
#include <filesystem> // ELI5: work with paths and folders.
#include <fstream> // ELI5: write the reconstructed file.
#include <initializer_list> // ELI5: pass small lists of allowed options.
#include <iostream> // ELI5: print help, results, and errors.
#include <map> // ELI5: collect named command options.
#include <set> // ELI5: detect repeated names and peer IDs.
#include <stdexcept> // ELI5: report command and storage errors.
#include <string> // ELI5: store command text and paths.
#include <string_view> // ELI5: compare command words without copying them.
#include <vector> // ELI5: keep flexible lists of arguments and peers.

namespace { // ELI5: keep command-line helpers private to this app.

struct Arguments { // ELI5: store the words and switches given on the command line.
    std::vector<std::string> positional; // ELI5: keep values that are not option names.
    std::map<std::string, std::string> options; // ELI5: map each option name to its value.
    bool force{}; // ELI5: remember whether replacing output was explicitly allowed.
};

Arguments parse_arguments(int argc, char* argv[], int start) {
    Arguments result; // ELI5: start with no parsed options or values.
    for (int i = start; i < argc; ++i) {
        const std::string token(argv[i]); // ELI5: read the next command-line word.
        if (token == "--force") {
            result.force = true; // ELI5: record permission to replace an output file.
        } else if (token.rfind("--", 0) == 0) {
            if (i + 1 >= argc) throw std::invalid_argument("missing value for " + token); // ELI5: every other option needs a value after it.
            if (!result.options.emplace(token, argv[++i]).second) {
                throw std::invalid_argument("option repeated: " + token); // ELI5: reject giving the same option more than once.
            }
        } else {
            result.positional.push_back(token); // ELI5: keep ordinary values like the file path or command argument.
        }
    }
    return result; // ELI5: return all the parsed command-line values.
}

const std::string& required(const Arguments& args, const std::string& option) {
    const auto found = args.options.find(option); // ELI5: look up the requested option name.
    if (found == args.options.end() || found->second.empty()) {
        throw std::invalid_argument("missing required option " + option); // ELI5: explain when a required value was not supplied.
    }
    return found->second; // ELI5: return the option's value.
}

void validate_options(const Arguments& args,
                      std::initializer_list<std::string_view> allowed,
                      bool allow_force) {
    for (const auto& [name, value] : args.options) { // ELI5: inspect every option the user supplied.
        (void)value; // ELI5: only the option name matters for this check.
        const auto found = std::find(allowed.begin(), allowed.end(), std::string_view(name));
        if (found == allowed.end()) throw std::invalid_argument("unsupported option " + name); // ELI5: reject options this command does not support.
    }
    if (args.force && !allow_force) throw std::invalid_argument("--force is only valid with rebuild"); // ELI5: only rebuild may overwrite an existing output.
}

std::vector<std::pair<std::string, cn::PeerIdentity>> parse_peers(const std::string& text) {
    std::vector<std::pair<std::string, cn::PeerIdentity>> peers; // ELI5: prepare the named pretend-peer list.
    std::set<std::string> names; // ELI5: keep names already used so they cannot repeat.
    std::set<cn::PeerId> ids; // ELI5: keep generated IDs so collisions can be detected.
    std::size_t start = 0; // ELI5: begin reading the first comma-separated name.
    while (start <= text.size()) {
        const auto comma = text.find(',', start); // ELI5: find the next separator.
        const auto end = comma == std::string::npos ? text.size() : comma; // ELI5: stop at that separator or the end of the text.
        const auto name = text.substr(start, end - start); // ELI5: take one peer name from the list.
        if (name.empty() || name.size() > 32) throw std::invalid_argument("peer names must be 1 to 32 bytes"); // ELI5: keep each demo label short and non-empty.
        if (std::any_of(name.begin(), name.end(), [](unsigned char c) { return c <= 0x20 || c == 0x7f; })) {
            throw std::invalid_argument("peer names cannot contain whitespace or control bytes"); // ELI5: keep labels simple to read in the output.
        }
        if (!names.insert(name).second) throw std::invalid_argument("peer names must be unique"); // ELI5: do not allow two peers with the same label.

        const auto digest = cn::hashing::sha256(name); // ELI5: make a repeatable hash from the peer's name.
        cn::PeerId id = (static_cast<cn::PeerId>(digest[0]) << 24) |
                        (static_cast<cn::PeerId>(digest[1]) << 16) |
                        (static_cast<cn::PeerId>(digest[2]) << 8) |
                        static_cast<cn::PeerId>(digest[3]);
        if (id == 0) id = 1; // ELI5: reserve zero by changing this rare result to one.
        if (!ids.insert(id).second) throw std::invalid_argument("peer name hash collision; choose other labels"); // ELI5: refuse two labels that accidentally make the same peer number.
        peers.emplace_back(name, cn::PeerIdentity{id, 1}); // ELI5: save this label with a stable demo identity.
        if (comma == std::string::npos) break; // ELI5: stop after the last name.
        start = comma + 1; // ELI5: start parsing the text after the comma.
    }
    return peers; // ELI5: return the validated pretend-peer list.
}

void print_help() {
    std::cout << "chunk_tool: M3 chunking, hashing, local storage and placement demo\n" // ELI5: name what the tool demonstrates...
              << "Usage:\n"
              << "  chunk_tool share <file> --data-dir <dir> [--peers A,B,C,D]\n"
              << "  chunk_tool rebuild <file-id> --data-dir <dir> --output <file> [--force]\n"
              << "  chunk_tool catalog --data-dir <dir>\n"
              << "  chunk_tool --help | --version\n\n"
              << "Managed chunks are written under <dir>/chunk_store; source files are untouched.\n"; // ELI5: explain where data goes and that the input is not changed.
}

void print_manifest(const cn::FileManifest& manifest) {
    std::cout << "file_id: " << cn::hashing::to_hex(manifest.file_id) << '\n' // ELI5: show the whole-file hash used as its ID...
              << "name: " << manifest.display_name << '\n'
              << "file_size: " << manifest.file_size << " bytes\n"
              << "chunk_size: " << manifest.chunk_size << " bytes\n"
              << "chunk_count: " << manifest.chunk_count << '\n'
              << "final_chunk_size: " << manifest.final_chunk_size << " bytes\n"; // ELI5: show the shorter last piece's exact length.
}

int share_file(const Arguments& args) {
    validate_options(args, {"--data-dir", "--peers"}, false); // ELI5: allow only the data-folder and pretend-peer options.
    if (args.positional.size() != 1) throw std::invalid_argument("share requires exactly one source file"); // ELI5: require one input filename.
    const auto& data_dir = required(args, "--data-dir"); // ELI5: get the folder where demo storage will be written.
    const auto peer_arg = args.options.find("--peers"); // ELI5: check whether custom pretend peers were provided.
    const auto peers = parse_peers(peer_arg == args.options.end() ? "A,B,C,D" : peer_arg->second); // ELI5: use four defaults or validate the supplied labels.

    const auto manifest = cn::filestore::build_manifest(args.positional.front()); // ELI5: calculate the file and per-piece hashes.
    cn::filestore::ChunkStore store(std::filesystem::path(data_dir) / "chunk_store"); // ELI5: open storage beneath the chosen data folder.
    store.store_file(args.positional.front(), manifest); // ELI5: verify and save the file's chunks locally.
    const auto plan = cn::filestore::make_placement_plan(manifest, [&] {
        std::vector<cn::PeerIdentity> identities; // ELI5: prepare just the identity part of the named peers.
        identities.reserve(peers.size()); // ELI5: make enough room for each peer.
        for (const auto& peer : peers) identities.push_back(peer.second); // ELI5: copy each identity into the list.
        return identities; // ELI5: give the placement planner its peer list.
    }());

    print_manifest(manifest); // ELI5: show the file details before the assignment table.
    std::cout << "placement: " << peers.size() << " pretend peers; assignments below are intended holders\n"
              << "chunk  size  SHA-256                                                           holders\n";
    for (const auto& placement : plan.chunks) {
        std::cout << placement.chunk_index << "  " // ELI5: print the piece number...
                  << cn::filestore::expected_chunk_size(manifest, placement.chunk_index) << "  "
                  << cn::hashing::to_hex(manifest.chunk_hashes[placement.chunk_index]) << "  ";
        for (std::size_t i = 0; i < placement.holders.size(); ++i) { // ELI5: print each pretend peer assigned to keep this piece.
            const auto found = std::find_if(peers.begin(), peers.end(), [&](const auto& peer) { // ELI5: match the planned identity back to its readable label.
                return peer.second.id == placement.holders[i].id; // ELI5: compare the peer number.
            }); // ELI5: finish searching the named peer list.
            if (i != 0) std::cout << ','; // ELI5: separate multiple holder names with commas.
            std::cout << (found == peers.end() ? std::to_string(placement.holders[i].id) : found->first); // ELI5: print its number if there is no label, otherwise print the label.
        }
        if (placement.holders.size() < 2) std::cout << " (degraded: only one target)"; // ELI5: warn if this piece has only one planned copy.
        std::cout << '\n'; // ELI5: start the next row on a fresh line.
    }
    if (manifest.chunk_count != 0 && !plan.avoids_a_complete_managed_copy) {
        std::cout << "warning: current peer/file count cannot enforce the no-complete-managed-copy invariant\n"; // ELI5: warn when the peer count cannot spread all pieces apart.
    }
    std::cout << "local demo note: all source chunks remain stored here; remote placement is not confirmed\n"; // ELI5: clarify this tool only simulates remote assignments.
    return 0; // ELI5: report that the share demo completed.
}

int rebuild_file(const Arguments& args) {
    validate_options(args, {"--data-dir", "--output"}, true); // ELI5: allow the storage folder and output path, plus optional overwrite permission.
    if (args.positional.size() != 1) throw std::invalid_argument("rebuild requires one 64-digit file ID"); // ELI5: require exactly one file ID.
    const auto& data_dir = required(args, "--data-dir"); // ELI5: find the managed-data folder.
    const auto& output_name = required(args, "--output"); // ELI5: find where the restored file should be written.
    const auto file_id = cn::hashing::from_hex(args.positional.front()); // ELI5: turn the text ID into hash bytes.
    cn::filestore::ChunkStore store(std::filesystem::path(data_dir) / "chunk_store"); // ELI5: open the saved piece store.
    const auto manifest = store.find_manifest(file_id); // ELI5: look up the file's piece order and checksums.
    if (!manifest) throw std::runtime_error("file ID is not in the local catalog"); // ELI5: stop if no file details are available.

    const std::filesystem::path output_path(output_name); // ELI5: make the output text into a path object.
    if (std::filesystem::is_directory(output_path)) {
        throw std::runtime_error("output path names a directory"); // ELI5: a file cannot be reconstructed over a folder.
    }
    if (std::filesystem::exists(output_path) && !args.force) {
        throw std::runtime_error("output already exists; pass --force to replace it"); // ELI5: protect existing files unless overwrite was chosen.
    }
    if (!output_path.parent_path().empty()) std::filesystem::create_directories(output_path.parent_path()); // ELI5: create parent folders if needed.
    auto temporary = output_path; // ELI5: build a temporary path beside the final output.
    temporary += ".part-" + std::to_string( // ELI5: add a suffix so unfinished output is not mistaken for a complete file.
        std::chrono::steady_clock::now().time_since_epoch().count()); // ELI5: use a changing time value to help make the name unique.
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc); // ELI5: write a fresh temporary file as raw bytes.
    if (!output) throw std::runtime_error("could not create temporary output: " + temporary.string()); // ELI5: stop if the temporary file cannot be opened.

    cn::hashing::Sha256 full_hash; // ELI5: hash the restored whole file as pieces are written.
    try {
        for (std::uint32_t index = 0; index < manifest->chunk_count; ++index) { // ELI5: rebuild the file in its original piece order.
            cn::Bytes chunk; // ELI5: prepare a place to receive one piece.
            const auto error = store.read_chunk(cn::ChunkKey{file_id, index}, chunk); // ELI5: load and verify that piece from local storage.
            if (error) throw std::runtime_error("chunk " + std::to_string(index) + " failed verification: " + error.message()); // ELI5: stop if the piece is missing or damaged.
            full_hash.update(chunk); // ELI5: add this piece to the whole-file checksum.
            output.write(reinterpret_cast<const char*>(chunk.data()), static_cast<std::streamsize>(chunk.size())); // ELI5: append these bytes to the temporary restored file.
            if (!output) throw std::runtime_error("failed writing reconstructed output"); // ELI5: report if the disk could not accept the piece.
        }
        output.flush(); // ELI5: ask the operating system to finish writing.
        if (!output) throw std::runtime_error("failed flushing reconstructed output"); // ELI5: report if the last bytes could not be saved.
        output.close(); // ELI5: close the complete temporary file before publishing it.
        if (full_hash.finish() != manifest->file_id) {
            throw std::runtime_error("reconstructed file SHA-256 does not match the manifest"); // ELI5: require the whole restored file to match the advertised ID.
        }
        std::error_code rename_error; // ELI5: reserve space for a publish error.
        std::filesystem::rename(temporary, output_path, rename_error); // ELI5: give the checked temporary file its final name.
        if (rename_error) throw std::filesystem::filesystem_error("could not publish reconstructed output", temporary, output_path, rename_error); // ELI5: report if publication failed.
    } catch (...) {
        output.close(); // ELI5: close any unfinished output first.
        std::error_code ignored; // ELI5: keep cleanup trouble from hiding the original error.
        std::filesystem::remove(temporary, ignored); // ELI5: delete the partial file after a failure.
        throw; // ELI5: pass the original failure to the command handler.
    }
    std::cout << "verified: " << output_path.string() << '\n' // ELI5: tell the user where the restored file was written...
              << "file_id: " << cn::hashing::to_hex(file_id) << '\n'
              << "bytes: " << manifest->file_size << '\n'; // ELI5: ...and show its expected byte count.
    return 0; // ELI5: report a successful rebuild.
}

int print_catalog(const Arguments& args) {
    validate_options(args, {"--data-dir"}, false); // ELI5: allow only the managed-data folder option.
    if (!args.positional.empty()) throw std::invalid_argument("catalog takes no positional arguments"); // ELI5: this command does not accept extra words.
    const auto& data_dir = required(args, "--data-dir"); // ELI5: get the managed-data folder.
    cn::filestore::ChunkStore store(std::filesystem::path(data_dir) / "chunk_store"); // ELI5: open the local M3 data.
    const auto manifests = store.load_manifests(); // ELI5: read every saved file description.
    if (manifests.empty()) {
        std::cout << "catalog is empty\n"; // ELI5: show that no files have been shared into this store.
        return 0; // ELI5: an empty catalog is still a successful query.
    }
    for (const auto& manifest : manifests) {
        print_manifest(manifest); // ELI5: show the saved file facts.
        const auto presence = store.local_presence(manifest.file_id); // ELI5: check every piece on disk and verify it.
        const auto count = std::count(presence.begin(), presence.end(), static_cast<std::uint8_t>(1)); // ELI5: count pieces that passed their checks.
        std::cout << "locally_verified_chunks: " << count << '/' << manifest.chunk_count << "\n\n";
    }
    return 0; // ELI5: report that listing the catalog succeeded.
}

} // ELI5: finish the command-specific helper functions.

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") { // ELI5: handle a request to show the version.
        std::cout << cn::project_version << '\n'; // ELI5: print the shared version label.
        return 0; // ELI5: report success.
    }
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) { // ELI5: show help when no command or only --help is given.
        print_help(); // ELI5: print the available commands and options.
        return 0; // ELI5: help is a successful request.
    }
    try {
        const std::string command(argv[1]); // ELI5: read the first word as the command name.
        const auto args = parse_arguments(argc, argv, 2); // ELI5: parse the remaining words as its arguments.
        if (command == "share") return share_file(args); // ELI5: run the chunk-and-store demonstration.
        if (command == "rebuild") return rebuild_file(args); // ELI5: rebuild a file from its verified pieces.
        if (command == "catalog") return print_catalog(args); // ELI5: list locally saved file descriptions.
        throw std::invalid_argument("unknown command: " + command); // ELI5: tell the user if no command matched.
    } catch (const std::exception& error) {
        std::cerr << "chunk_tool: " << error.what() << '\n'; // ELI5: print a readable reason the command failed.
        return 2; // ELI5: signal an invalid command or operation failure.
    }
}
