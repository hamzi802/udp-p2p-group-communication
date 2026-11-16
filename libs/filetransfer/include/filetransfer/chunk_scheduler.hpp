#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cn::filetransfer {

using ChunkId = std::uint32_t;

struct Assignment {
    ChunkId chunk;
    std::string source;
};

// Decides which source should supply which chunk and re-assigns
// unfinished chunks to another holder when a source fails.
class ChunkScheduler {
public:
    // holders[i] = names of the peers that hold chunk i
    // maxPerSource / maxTotal bound how many requests may be in flight.
    explicit ChunkScheduler(std::vector<std::vector<std::string>> holders,
                            std::size_t maxPerSource = 2,
                            std::size_t maxTotal = 4);

    // Next pending chunk and the least-busy usable holder, if any.
    std::optional<Assignment> next();

    void markDone(ChunkId chunk);
    // Timeout or bad hash: try another holder for this chunk.
    void retryElsewhere(ChunkId chunk);
    // Source left or crashed: its unfinished chunks become pending again.
    void markSourceDown(const std::string& source);

    bool finished() const;  // every chunk is Done
    bool failed() const;    // some chunk has no usable holder left

private:
    enum class State { Pending, InFlight, Done, Failed };
    struct Chunk {
        std::vector<std::string> holders;
        std::set<std::string> tried;
        State state = State::Pending;
        std::string source;
    };
    std::size_t inFlight(const std::string& source) const;
    std::size_t totalInFlight() const;

    std::size_t maxPerSource_;
    std::size_t maxTotal_;

    std::vector<Chunk> chunks_;
    std::set<std::string> down_;
};

}  // namespace cn::filetransfer
