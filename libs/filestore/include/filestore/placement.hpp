#pragma once // ELI5: prevent duplicate inclusion.

#include "common/file_types.hpp" // ELI5: reuse the shared peer and file-ID types.

#include <vector> // ELI5: hold lists of peers and chunk assignments.

namespace cn::filestore { // ELI5: group file-placement tools together.

struct ChunkPlacement { // ELI5: say which peers should keep one piece.
    std::uint32_t chunk_index{}; // ELI5: the piece number in the file.
    std::vector<PeerIdentity> holders; // ELI5: the peers assigned to store this piece.
};

struct PlacementPlan { // ELI5: describe the full piece-storage plan for one file.
    std::vector<PeerIdentity> ordered_peers; // ELI5: the stable peer order used to make assignments.
    std::vector<ChunkPlacement> chunks; // ELI5: the holder list for each piece.
    bool has_two_distinct_holders{}; // ELI5: true when every piece can have two different keepers.
    bool avoids_a_complete_managed_copy{}; // ELI5: true when no peer is assigned every piece.
};

// ELI5: Sort peers and rotate assignments using the file ID so the plan is repeatable.
// ELI5: Fewer than two peers cannot provide two copies; exactly two peers must each keep all pieces.
PlacementPlan make_placement_plan(const FileManifest& manifest,
                                 std::vector<PeerIdentity> active_peers); // ELI5: calculate where each piece should live.

} // namespace cn::filestore: the placement plan interface is declared.
