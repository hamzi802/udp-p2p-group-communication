#include "filestore/placement.hpp" // ELI5: declare the placement-plan types and function.

#include "filestore/manifest.hpp" // ELI5: use the rules that check file descriptions.

#include <algorithm> // ELI5: sort the peer list into a repeatable order.
#include <stdexcept> // ELI5: report invalid peer lists clearly.

namespace cn::filestore { // ELI5: keep this code in the file-storage area.

PlacementPlan make_placement_plan(const FileManifest& manifest,
                                 std::vector<PeerIdentity> active_peers) {
    validate_manifest(manifest, false); // ELI5: reject broken file details before planning.
    std::sort(active_peers.begin(), active_peers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.id < rhs.id; // ELI5: put smaller peer numbers first.
    }); // ELI5: sort so every peer calculates the same plan.
    for (std::size_t i = 1; i < active_peers.size(); ++i) {
        if (active_peers[i - 1].id == active_peers[i].id) {
            throw std::invalid_argument("active membership contains duplicate peer IDs"); // ELI5: two sessions must not claim the same peer number.
        }
    }

    PlacementPlan plan; // ELI5: start with an empty plan.
    plan.ordered_peers = std::move(active_peers); // ELI5: keep the sorted peers in the plan.
    const auto peer_count = plan.ordered_peers.size(); // ELI5: count how many peers can store pieces.
    plan.has_two_distinct_holders = peer_count >= 2; // ELI5: two separate copies need at least two peers.
    plan.avoids_a_complete_managed_copy = peer_count >= 3 && manifest.chunk_count >= 3; // ELI5: enough peers and pieces let us spread work without one peer holding everything.
    plan.chunks.reserve(manifest.chunk_count); // ELI5: make room for one assignment per piece.

    if (peer_count == 0) return plan; // ELI5: no peers means no assignments can be made.
    std::uint64_t id_prefix = 0; // ELI5: prepare a small number from the start of the file ID.
    for (std::size_t i = 0; i < 8; ++i) id_prefix = (id_prefix << 8) | manifest.file_id[i]; // ELI5: combine the first eight ID bytes into that number.
    const std::size_t offset = static_cast<std::size_t>(id_prefix % peer_count); // ELI5: use the ID to choose where assignments begin.

    for (std::uint32_t chunk = 0; chunk < manifest.chunk_count; ++chunk) {
        ChunkPlacement placement; // ELI5: make a holder list for this piece.
        placement.chunk_index = chunk; // ELI5: remember which piece this assignment describes.
        const auto first = (offset + chunk) % peer_count; // ELI5: rotate through peers so pieces are spread around.
        placement.holders.push_back(plan.ordered_peers[first]); // ELI5: give this piece its first keeper.
        if (peer_count >= 2) {
            const auto second = (first + 1) % peer_count; // ELI5: pick the next peer in the circle.
            placement.holders.push_back(plan.ordered_peers[second]); // ELI5: give this piece a second, different keeper.
        }
        plan.chunks.push_back(std::move(placement)); // ELI5: add this piece's assignment to the complete plan.
    }
    return plan; // ELI5: give the finished plan back to the caller.
}

} // namespace cn::filestore: placement planning is implemented here.
